#include "Globals.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "RuntimeCore.h"
#include <ole2.h>
#include "DragDropManager.h"
#include "EngineDropTarget.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ImGuiPass.h"
#include "ShadowMapPass.h"
#include "SceneGraph.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "ComponentLights.h"
#include "SceneViewPanel.h"
#include "GameViewPanel.h"
#include "PerformancePanel.h"
#include <d3dx12.h>
#include <functional>
#include <algorithm>

void ModuleEditor::preRender(){
    flushExitPrefabEdit();
    m_sceneView->handleResize();
    m_gameView->handleResize();
    m_imguiPass->startFrame();
    ImGuizmo::BeginFrame();
    handleShortcuts();
    const float dt = static_cast<float>(app->getElapsedMilis()) * 0.001f;

    RuntimeCore* runtimeCore = app->getRuntimeCore();

    // Keep the cull frustum's aspect ratio matched to the Game view so culling,
    // the debug frustum, and the actual Game render all agree.
    float aspect = 0.f;
    const ImVec2 gv = m_gameView->viewport.size;
    if (gv.x > 0.f && gv.y > 0.f) aspect = gv.x / gv.y;

    runtimeCore->tick(dt, aspect);

    if (m_effectsPlaying && runtimeCore->getSceneManager() &&
        runtimeCore->getSceneManager()->getState() != SceneManager::PlayState::Playing){
        updateEffectsInEditMode(dt);
    }

    m_performance->pushFPS(app->getFPS());

    DragDropManager::Get().Update();

    drawDockspace();
    drawMenuBar();
    if (m_sceneView) m_sceneView->visibleThisFrame = false;
    if (m_gameView)  m_gameView->visibleThisFrame = false;
    for (EditorPanel* p : m_panels) if (p->open) p->draw();



    handleDialogs();
    drawStatusBar();
    drawShadowMapPreview();

    drawDragDropOverlay();
}

void ModuleEditor::render(){
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleShaderDescriptors* descs = app->getShaderDescriptors();
    ID3D12GraphicsCommandList* cmd = d3d12->getCommandList();

    m_scriptWatcher.poll();

    m_frameTransientBuffers.clear();

    cmd->Reset(d3d12->getCommandAllocator(), nullptr);
    cmd->EndQuery(m_gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);

    ID3D12DescriptorHeap* heaps[] = { descs->getHeap(), app->getSamplerHeap()->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);
    handleNewScenePopup(cmd);

    if (m_sceneView->viewport.isReady() && m_sceneView->visibleThisFrame) m_sceneView->renderToTexture(cmd);
    if (m_gameView->viewport.isReady() && m_gameView->visibleThisFrame) m_gameView->renderToTexture(cmd);

    auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->getBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ResourceBarrier(1, &toRT);
    auto rtv = d3d12->getRenderTargetDescriptor();
    cmd->OMSetRenderTargets(1, &rtv, false, nullptr);
    float clear[] = { 0, 0, 0, 1 };
    cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);
    m_imguiPass->record(cmd);
    auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->getBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    cmd->ResourceBarrier(1, &toPresent);

    cmd->EndQuery(m_gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
    cmd->ResolveQueryData(m_gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, m_gpuReadback.Get(), 0);
    cmd->Close();

    ID3D12CommandList* lists[] = { cmd };
    d3d12->getDrawCommandQueue()->ExecuteCommandLists(1, lists);

    UINT64* data = nullptr;
    if (SUCCEEDED(m_gpuReadback->Map(0, nullptr, (void**)&data)) && data){
        UINT64 freq = 0;
        d3d12->getDrawCommandQueue()->GetTimestampFrequency(&freq);
        m_gpuFrameTimeMs = double(data[1] - data[0]) / double(freq) * 1000.0;
        m_gpuTimerReady = true;
        m_gpuReadback->Unmap(0, nullptr);
        m_performance->setGpuMs(m_gpuFrameTimeMs);
    }

    m_memoryUpdateTimer += (float)app->getElapsedMilis();
    if (m_memoryUpdateTimer >= 1000.0f){ m_memoryUpdateTimer = 0.0f; updateMemory(); }
}

void ModuleEditor::drawShadowMapPreview(){
    ShadowMapPass* shadowMapPass = app->getRuntimeCore()->getShadowMapPass();
    if (!shadowMapPass || !shadowMapPass->hasPreview()) return;
    SceneGraph* scene = getActiveModuleScene();
    if (!scene) return;

    ComponentDirectionalLight* caster = nullptr;
    std::function<void(GameObject*)> find = [&](GameObject* n){
        if (!n || !n->isActive() || caster) return;
        if (auto* dl = n->getComponent<ComponentDirectionalLight>(); dl && dl->enabled){
            caster = dl; return;
        }
        for (auto* c : n->getChildren()) find(c);
    };
    find(scene->getRoot());
    if (!caster || !caster->castShadows || !caster->shadowShowPreview) return;

    ImGui::SetNextWindowSize(ImVec2(360.f, 420.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Shadow Map", &caster->shadowShowPreview)){
        const int maxSlice = std::max(0, caster->shadowCascadeCount - 1);
        ImGui::SliderInt("Cascade", &caster->shadowPreviewCascade, 0, maxSlice);
        float size = std::min(ImGui::GetContentRegionAvail().x,
                              ImGui::GetContentRegionAvail().y);
        if (size > 16.f)
            ImGui::Image((ImTextureID)shadowMapPass->getPreviewSrvHandle().ptr,
                         ImVec2(size, size));
    }
    ImGui::End();
}
