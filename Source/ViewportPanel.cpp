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
