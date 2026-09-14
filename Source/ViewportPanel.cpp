#include "Globals.h"
#include "ViewportPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleStaticBuffer.h"
#include "ModuleResources.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "TonemapPass.h"
#include "BloomPass.h"
#include "FogPass.h"
#include "VolumetricFogPass.h"
#include "GBufferPass.h"
#include "PostProcessChain.h"
#include "SceneManager.h"
#include "EditorSceneSettings.h"
#include <algorithm>

void ViewportPanel::renderToTexture(ID3D12GraphicsCommandList* cmd){
    const uint32_t w = (uint32_t)viewport.size.x;
    const uint32_t h = (uint32_t)viewport.size.y;
    if (!viewport.rt || w == 0 || h == 0) return;
    Matrix view, proj;
    if (!buildCameraMatrices(w, h, view, proj)) return;
    ModuleStaticBuffer* sb = app->getStaticBuffer();
    if (sb && sb->isInitialized()){
        app->getResources()->uploadPendingMeshes(cmd, sb);
        sb->finalizeUploads(cmd);
    }
    viewport.rt->beginRender(cmd);
    m_editor->renderSceneWithCamera(cmd, view, proj, w, h, useEditorExtras(), viewport.rt.get());
    onPostRender(cmd, w, h);
    viewport.rt->endRender(cmd);

    PostProcessChain* chain = m_editor->getPostProcessChain();
    RenderTexture* hdrResult = viewport.rt.get();
    if (chain)
        hdrResult = chain->run(cmd, PostProcessEffectDef::Domain::PreTonemap, viewport.rt.get(), viewport.rtScratch.get());

    EditorSceneSettings* settings = nullptr;
    if (SceneManager* sm = m_editor->getSceneManager()) settings = &sm->getSettings();

    GBufferPass* gbuffer = m_editor->getGBufferPass();
    if (gbuffer && settings && settings->fog.enabled){
        Matrix invView; view.Invert(invView);
        Vector3 camPos = invView.Translation();
        Matrix invViewProj; (view * proj).Invert(invViewProj);
        RenderTexture* fogOut = (hdrResult == viewport.rt.get()) ? viewport.rtScratch.get() : viewport.rt.get();
        if (settings->fog.mode == EditorSceneSettings::Fog::Mode::Volumetric){
            if (VolumetricFogPass* volFog = m_editor->getVolumetricFogPass()){
                VolumetricFogSettings vfs;
                vfs.enabled = true;
                vfs.numSteps = (uint32_t)std::max(1, settings->fog.numSteps);
                vfs.extinctionCoeff = settings->fog.extinctionCoeff;
                vfs.noiseAmount = settings->fog.noiseAmount;
                vfs.fogIntensity = settings->fog.fogIntensity;
                vfs.anisotropyG = settings->fog.anisotropyG;
                vfs.maxOpacity = settings->fog.maxOpacity;
                vfs.halfResolution = settings->fog.halfResolution;
                vfs.boundedRayLength = settings->fog.boundedRayLength;
                const float elapsedTime = (float)app->getElapsedMilis() / 1000.f;
                hdrResult = volFog->render(cmd, hdrResult, fogOut, *gbuffer, camPos, view, proj,
                                           invViewProj, elapsedTime, m_editor->getFrameLights(),
                                           m_editor->getFrameShadowData(), vfs, /*viewportIndex=*/0);
            }
        } else if (FogPass* fog = m_editor->getFogPass()){
            FogSettings fs;
            fs.enabled = true;
            fs.mode = (settings->fog.mode == EditorSceneSettings::Fog::Mode::ExponentialHeight)
                          ? FogSettings::Mode::ExponentialHeight : FogSettings::Mode::Linear;
            fs.color = settings->fog.color;
            fs.startDistance = settings->fog.startDistance;
            fs.endDistance = settings->fog.endDistance;
            fs.maxOpacity = settings->fog.maxOpacity;
            fs.density = settings->fog.density;
            fs.heightFalloff = settings->fog.heightFalloff;
            fs.heightOffset = settings->fog.heightOffset;
            hdrResult = fog->render(cmd, hdrResult, fogOut, *gbuffer, camPos, invViewProj, fs, /*viewportIndex=*/0);
        }
    }

    BloomPass* bloom = m_editor->getBloomPass();
    RenderTexture* bloomResult = nullptr;
    if (bloom && settings && settings->postProcess.bloomEnabled){
        bloom->render(cmd, hdrResult, viewport, settings->postProcess.bloomThreshold);
        bloomResult = viewport.bloomMips[0].get();
    }

    const int nPostGamma = chain ? chain->countEnabled(PostProcessEffectDef::Domain::PostGamma) : 0;
    RenderTexture* tonemapTarget = (nPostGamma % 2 == 0) ? viewport.display.get() : viewport.displayScratch.get();
    RenderTexture* tonemapOther = (nPostGamma % 2 == 0) ? viewport.displayScratch.get() : viewport.display.get();

    TonemapParams tp;
    if (settings){
        tp.exposure = settings->postProcess.exposure;
        tp.bloomIntensity = settings->postProcess.bloomIntensity;
        tp.lutEnabled = settings->postProcess.lutEnabled;
    }
    if (TonemapPass* tonemap = m_editor->getTonemapPass())
        tonemap->render(cmd, hdrResult, tonemapTarget, bloomResult, m_editor->getColorLUT(), tp);

    if (chain && nPostGamma > 0)
        chain->run(cmd, PostProcessEffectDef::Domain::PostGamma, tonemapTarget, tonemapOther);
}

void ViewportPanel::handleResize(){
    if (!viewport.pendingResize) return;
    if (viewport.newWidth > 4 && viewport.newHeight > 4){
        app->getD3D12()->flush();
        viewport.rt->resize(viewport.newWidth, viewport.newHeight);
        viewport.rtScratch->resize(viewport.newWidth, viewport.newHeight);
        viewport.display->resize(viewport.newWidth, viewport.newHeight);
        viewport.displayScratch->resize(viewport.newWidth, viewport.newHeight);

        uint32_t mw = viewport.newWidth, mh = viewport.newHeight;
        for (int i = 0; i < EditorViewport::kNumBloomMips; ++i){
            mw = std::max(1u, mw / 2);
            mh = std::max(1u, mh / 2);
            viewport.bloomMips[i]->resize(mw, mh);
        }

        onResized(viewport.newWidth, viewport.newHeight);
    }
    viewport.pendingResize = false;
}

void ViewportPanel::drawContent(){
    visibleThisFrame = true;
    viewport.size = ImGui::GetContentRegionAvail();
    viewport.checkResize();
    if (viewport.isReady()){
        ImGui::Image((ImTextureID)viewport.display->getSrvHandle().ptr, viewport.size);
        viewport.pos = ImGui::GetItemRectMin();
        onImageDrawn();
    }
    else textMuted("%s", notReadyText());
    onDrawOverlays();
}
