#include "Globals.h"
#include "ModuleEditor.h"
#include "Application.h"
#include <ole2.h>
#include "DragDropManager.h"
#include "EngineDropTarget.h"
#include "ModuleD3D12.h"
#include "ComponentAnimation.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ImGuiPass.h"
#include "DebugDrawPass.h"
#include "MeshRenderPass.h"
#include "GBufferPass.h"
#include "DeferredLightingPass.h"
#include "RenderTexture.h"
#include "EmptyScene.h"
#include "ModuleScene.h"
#include "SceneManager.h"
#include "ShaderTableDesc.h"
#include "MeshPipeline.h"
#include "FileDialog.h"
#include "EditorSceneSettings.h"
#include "EnvironmentSystem.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ComponentCamera.h"
#include "ComponentLights.h"
#include "ComponentFactory.h"
#include "PrimitiveFactory.h"
#include "ModuleCamera.h"
#include "FrustumDebugDraw.h"
#include "BoundingVolume.h"
#include "ComponentBounds.h"
#include "ComponentRigidbody.h"
#include "CollisionSystem.h"
#include "CollisionResponse.h"
#include "EnvironmentMap.h"
#include "SceneViewPanel.h"
#include "GameViewPanel.h"
#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "AssetBrowserPanel.h"
#include "SceneSettingsPanel.h"
#include "GPUMemoryPanel.h"
#include "RenderGraphPanel.h"
#include "PrefabManager.h"
#include "FileWatcher.h"
#include "ResourceMaterial.h"
#include "ResourceModel.h"
#include "ModuleResources.h"
#include "ModuleAssets.h"
#include "Model.h"
#include "Mesh.h"
#include "MeshEntry.h"
#include "ResourceMesh.h"
#include <d3dx12.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
static constexpr float kDeg2Rad = 0.0174532925f;

ModuleEditor::ModuleEditor() = default;
ModuleEditor::~ModuleEditor() = default;

ComPtr<ID3D12Resource> ModuleEditor::createUploadBuffer(ID3D12Device* device, SIZE_T size, const wchar_t* name) {
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bd = CD3DX12_RESOURCE_DESC::Buffer((size + 255) & ~255);
    ComPtr<ID3D12Resource> buf;
    device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf));
    if (name) buf->SetName(name);
    return buf;
}

bool ModuleEditor::init() {
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleShaderDescriptors* descs = app->getShaderDescriptors();
    ID3D12Device* device = d3d12->getDevice();
    m_descTable = descs->allocTable();

    ComPtr<ID3D12Device2> device2;
    ComPtr<ID3D12Device4> device4;
    device->QueryInterface(IID_PPV_ARGS(&device2));
    device->QueryInterface(IID_PPV_ARGS(&device4));

    m_imguiPass = std::make_unique<ImGuiPass>(device2.Get(), d3d12->getHWnd(), m_descTable.getCPUHandle(), m_descTable.getGPUHandle());
    m_debugDraw = std::make_unique<DebugDrawPass>(device4.Get(), d3d12->getDrawCommandQueue(), false);
    m_collisionSystem  = std::make_unique<CollisionSystem>();
    m_collisionResponse = std::make_unique<CollisionResponse>();
    m_sceneManager = std::make_unique<SceneManager>();
    m_meshRenderPass = std::make_unique<MeshRenderPass>();
    m_hotReload = std::make_unique<HotReloadManager>();

    // Register the callback. When a DLL reloads, every ScriptComponent in the
    // scene is told to swap its IScript* for the new version.
    m_hotReload->setReloadCallback([this](const std::string& dllPath) {
        notifyScriptComponentsReload(dllPath);
        });

    // Point the watcher at  build/PhoenixEngine/Debug/x64/Assets/Scripts/
    // (App->GetFileSystem()->GetAssetsPath() should return that path already.)
    std::string scriptDir = app->getFileSystem()->GetAssetsPath() + std::string("Scripts/");
    app->getFileSystem()->CreateDir(scriptDir.c_str());

    m_scriptWatcher.start(scriptDir,
        [this](const std::string& absPath, FileWatcher::Event ev) {
            onScriptFileEvent(absPath, ev);
        });

    // Load any DLLs that are already in Assets/Scripts/ when the engine starts.
    // This means you don't have to rebuild GameScripts just to get it recognised.
    auto existing = app->getFileSystem()->GetFilesInDirectory(scriptDir.c_str(), ".dll");
    for (const auto& path : existing)
        m_hotReload->loadLibrary(path);

    if (!m_meshRenderPass->init(device)) return false;

    m_skinningPass = std::make_unique<SkinningPass>();
    if (!m_skinningPass->init(device)) {
        // Non-fatal: editor runs without GPU skinning
        m_skinningPass.reset();
    }

    m_gbufferPass = std::make_unique<GBufferPass>();
    if (!m_gbufferPass->init(device)) return false;

    m_deferredLightingPass = std::make_unique<DeferredLightingPass>();
    if (!m_deferredLightingPass->init(device)) return false;

    m_envSystem = std::make_unique<EnvironmentSystem>();
    if (!m_envSystem->init(device, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT, false)) return false;

    m_sceneManager->setScene(std::make_unique<EmptyScene>(), device);

    D3D12_QUERY_HEAP_DESC qd = { D3D12_QUERY_HEAP_TYPE_TIMESTAMP, 2, 0 };
    device->CreateQueryHeap(&qd, IID_PPV_ARGS(&m_gpuQueryHeap));
    auto rbH = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    auto rbD = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * 2);
    device->CreateCommittedResource(&rbH, D3D12_HEAP_FLAG_NONE, &rbD, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_gpuReadback));

    m_saveDialog = std::make_unique<FileDialog>();
    m_loadDialog = std::make_unique<FileDialog>();
    m_saveDialog->setExtensionFilter(".json");
    m_loadDialog->setExtensionFilter(".json");

    m_sceneView = addPanel<SceneViewPanel>(this);
    m_gameView = addPanel<GameViewPanel>(this);
    addPanel<HierarchyPanel>(this);
    addPanel<InspectorPanel>(this);
    m_console = addPanel<ConsolePanel>(this);
    m_performance = addPanel<PerformancePanel>(this);
    m_assetBrowser = addPanel<AssetBrowserPanel>(this);
    addPanel<SceneSettingsPanel>(this);
    addPanel<ResourcesPanel>(this);
    addPanel<CollisionDebugPanel>(this);
    m_gpuMemory   = addPanel<GPUMemoryPanel>(this);
    m_renderGraph = addPanel<RenderGraphPanel>(this);

    // Register OLE drop target so we get DragEnter/DragOver/DragLeave/Drop
    // callbacks that drive the real-time drag overlay and background imports.
    HWND hwnd = app->getD3D12()->getHWnd();
    m_dropTarget = new EngineDropTarget();
    if (FAILED(RegisterDragDrop(hwnd, m_dropTarget))) {
        m_dropTarget->Release();
        m_dropTarget = nullptr;
        log("[Editor] RegisterDragDrop failed — drag-drop overlay disabled", EditorColors::Warning);
    }

    // When background imports finish, mark the asset browser dirty so it
    // re-scans its directory without needing a restart.
    DragDropManager::Get().SetRefreshCallback([this]() {
        if (m_assetBrowser) m_assetBrowser->markDirty();
    });

    log("[Editor] Initialized", EditorColors::Success);
    return true;
}

bool ModuleEditor::cleanUp() {
    // Shut down background import worker before any module state is torn down.
    DragDropManager::Get().Shutdown();

    // Revoke the OLE drop target before destroying the window resources.
    if (m_dropTarget) {
        RevokeDragDrop(app->getD3D12()->getHWnd());
        m_dropTarget->Release();
        m_dropTarget = nullptr;
    }

    m_ownedPanels.clear();
    m_panels.clear();
    m_envSystem.reset();
    m_imguiPass.reset();
    m_debugDraw.reset();
    m_sceneManager.reset();
    m_gbufferPass.reset();
    m_deferredLightingPass.reset();
    if (m_skinningPass) { m_skinningPass->cleanUp(); m_skinningPass.reset(); }
    m_gpuQueryHeap.Reset();
    m_gpuReadback.Reset();

    m_scriptWatcher.stop();
    m_hotReload->unloadAll();

    return true;
}

ModuleScene* ModuleEditor::getActiveModuleScene() const {
    return m_sceneManager ? m_sceneManager->getModuleScene() : nullptr;
}

void ModuleEditor::log(const char* text, const ImVec4& color) {
    if (m_console) m_console->add(text, color);
}

void ModuleEditor::preRender() {
    flushExitPrefabEdit();
    m_sceneView->handleResize();
    m_gameView->handleResize();
    m_imguiPass->startFrame();
    ImGuizmo::BeginFrame();
    handleShortcuts();
    const float dt = static_cast<float>(app->getElapsedMilis()) * 0.001f;

    if (m_sceneManager) {
        m_sceneManager->update(dt);
        m_sceneManager->updateAnimations(dt);
    }

    // Run the three-stage collision pipeline every frame (feeds the debug panel).
    // dt is forwarded so fast-moving bodies can use swept AABBs in the broad phase.
    ModuleScene* activeScene = getActiveModuleScene();
    if (m_collisionSystem)
        m_collisionSystem->run(activeScene, dt);

    // Apply position correction and velocity impulses — only during Play.
    const bool isPlaying = m_sceneManager &&
        m_sceneManager->getState() == SceneManager::PlayState::Playing;
    if (isPlaying && m_collisionResponse && m_collisionSystem)
        m_collisionResponse->solve(m_collisionSystem->getResults().contacts, dt);

    m_performance->pushFPS(app->getFPS());

    // Tick drag-drop manager: detects worker completion, triggers refresh
    // callback, and expires the "complete" progress banner.
    DragDropManager::Get().Update();

    drawDockspace();
    drawMenuBar();
    for (EditorPanel* p : m_panels) if (p->open) p->draw();
    drawFrameTimingBar();
    drawStatusBar();

    // --- Morph Target Debug Window ---
    {
        ModuleScene* morphScene = getActiveModuleScene();
        if (morphScene) {
            struct MorphEntry { ComponentMesh* cm; Mesh* mesh; std::string goName; GameObject* go; };
            std::vector<MorphEntry> found;
            std::function<void(GameObject*)> collect = [&](GameObject* node) {
                if (!node || !node->isActive()) return;
                if (auto* cm = node->getComponent<ComponentMesh>()) {
                    for (const auto& entry : cm->getEntries()) {
                        Mesh* m = entry.meshRes ? entry.meshRes->getMesh() : nullptr;
                        if (m && m->hasMorphTargets()) {
                            found.push_back({ cm, m, node->getName(), node });
                            break;
                        }
                    }
                }
                for (auto* c : node->getChildren()) collect(c);
            };
            collect(morphScene->getRoot());

            if (!found.empty()) {
                ImGui::Begin("Morph Targets Debug");
                for (auto& mt : found) {
                    const uint32_t n = mt.mesh->getNumMorphTargets();

                    // Walk up parent chain to find the ComponentAnimation driving this node.
                    ComponentAnimation* animComp = nullptr;
                    for (GameObject* p = mt.go->getParent(); p && !animComp; p = p->getParent())
                        animComp = p->getComponent<ComponentAnimation>();

                    const bool isPlaying = animComp && animComp->getController().isPlaying();

                    ImGui::Text("%s  |  Morph Targets: %u", mt.goName.c_str(), n);

                    if (animComp) {
                        float t = animComp->getController().CurrentTime;
                        bool hasMC = animComp->getController().hasMorphChannel(mt.goName.c_str());
                        ImGui::Text("Anim time: %.3f s  |  MorphChannel: %s",
                                    t, hasMC ? "YES" : "no (weights channel missing)");
                        if (!hasMC)
                            ImGui::TextColored(ImVec4(1,0.6f,0,1),
                                "  Check: node name in .anim matches '%s'", mt.goName.c_str());
                    } else {
                        ImGui::TextDisabled("  No ComponentAnimation in parent chain");
                    }
                    ImGui::Separator();

                    if (isPlaying) {
                        ImGui::BeginDisabled();
                        ImGui::TextDisabled("(animation is driving weights)");
                    }
                    for (uint32_t i = 0; i < n; ++i) {
                        char label[48];
                        snprintf(label, sizeof(label), "Weight %u##%s%u", i, mt.goName.c_str(), i);
                        float v = mt.cm->getMorphWeights()[i];
                        if (ImGui::SliderFloat(label, &v, 0.f, 1.f))
                            mt.cm->setMorphWeight((int)i, v);
                    }
                    if (isPlaying) ImGui::EndDisabled();

                    {
                        char wbuf[256] = {};
                        int off = 0;
                        for (uint32_t i = 0; i < n && off < (int)sizeof(wbuf) - 12; ++i)
                            off += snprintf(wbuf + off, sizeof(wbuf) - off,
                                            i ? "  %.3f" : "%.3f", mt.cm->getMorphWeights()[i]);
                        ImGui::Text("Weights this frame:  %s", wbuf);
                    }
                    ImGui::Spacing();
                }
                ImGui::End();
            }
        }
    }
    // --- End Morph Target Debug Window ---

    // --- Animation Debug Window ---
    {
        static bool s_showAnimDebug = true;
        ModuleScene* animScene = getActiveModuleScene();
        if (animScene && s_showAnimDebug) {
            struct AnimEntry { GameObject* go; ComponentAnimation* anim; };
            std::vector<AnimEntry> entries;
            std::function<void(GameObject*)> collectAnims = [&](GameObject* node) {
                if (!node || !node->isActive()) return;
                if (auto* anim = node->getComponent<ComponentAnimation>())
                    entries.push_back({ node, anim });
                for (auto* c : node->getChildren()) collectAnims(c);
            };
            collectAnims(animScene->getRoot());

            if (!entries.empty()) {
                if (ImGui::Begin("Animation Debug", &s_showAnimDebug)) {
                for (auto& e : entries) {
                    ImGui::PushID(e.go);
                    ImGui::Text("%s", e.go->getName().c_str());
                    ImGui::Indent();

                    ResourceStateMachine* sm = e.anim->getStateMachine();
                    if (sm) {
                        // SM-driven character
                        const HashString& active = e.anim->getActiveState();
                        ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "State: %s",
                            active.empty() ? "(none)" : active.str.c_str());
                        ImGui::Text("Layers: %d", e.anim->getLayerCount());

                        const AnimLayer* head = e.anim->getLayerHead();
                        if (head) {
                            ImGui::Text("Head time: %.1f ms", head->currentTimeMs);
                            float w = (head->transitionTimeMs > 0.f)
                                ? std::min(1.f, head->fadeTimeMs / head->transitionTimeMs)
                                : 1.f;
                            ImGui::Text("Blend weight: %.2f", w);
                        }

                        ImGui::Spacing();
                        ImGui::TextDisabled("Triggers:");
                        static const char* kTriggers[] = { "move", "stop", "run", "walk", "die" };
                        for (const char* t : kTriggers) {
                            if (ImGui::SmallButton(t))
                                e.anim->SendTrigger(HashString(std::string(t)));
                            ImGui::SameLine();
                        }
                        ImGui::NewLine();
                    } else {
                        // Morph-target face or direct-play animation
                        float t = e.anim->getController().CurrentTime;
                        ImGui::Text("Anim time: %.3f s", t);

                        // Walk children to find morph weights
                        std::function<void(GameObject*)> showMorphs = [&](GameObject* node) {
                            if (auto* cm = node->getComponent<ComponentMesh>()) {
                                for (const auto& en : cm->getEntries()) {
                                    if (!en.meshRes) continue;
                                    const uint32_t n = en.meshRes->getNumMorphTargets();
                                    if (n > 0) {
                                        ImGui::Text("  %s:", node->getName().c_str());
                                        const float* w = cm->getMorphWeights();
                                        for (uint32_t i = 0; i < n && i < 8; ++i)
                                            ImGui::Text("    Target %u: %.3f", i, w[i]);
                                        break;
                                    }
                                }
                            }
                            for (auto* child : node->getChildren()) showMorphs(child);
                        };
                        for (auto* child : e.go->getChildren()) showMorphs(child);
                    }

                    ImGui::Unindent();
                    ImGui::Separator();
                    ImGui::PopID();
                }
                } // end if (ImGui::Begin)
                ImGui::End();
            }
        }
    }
    // --- End Animation Debug Window ---

    handleDialogs();

    // Drag-drop overlay: drawn last so it sits on top of all panels.
    drawDragDropOverlay();
}

void ModuleEditor::render() {
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

    if (m_sceneView->viewport.isReady()) m_sceneView->renderToTexture(cmd);
    if (m_gameView->viewport.isReady()) m_gameView->renderToTexture(cmd);

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
    if (SUCCEEDED(m_gpuReadback->Map(0, nullptr, (void**)&data)) && data) {
        UINT64 freq = 0;
        d3d12->getDrawCommandQueue()->GetTimestampFrequency(&freq);
        m_gpuFrameTimeMs = double(data[1] - data[0]) / double(freq) * 1000.0;
        m_gpuTimerReady = true;
        m_gpuReadback->Unmap(0, nullptr);
        m_performance->setGpuMs(m_gpuFrameTimeMs);
    }

    m_memoryUpdateTimer += (float)app->getElapsedMilis();
    if (m_memoryUpdateTimer >= 1000.0f) { m_memoryUpdateTimer = 0.0f; updateMemory(); }
}

void ModuleEditor::renderSceneWithCamera(ID3D12GraphicsCommandList* cmd, const Matrix& view, const Matrix& proj, uint32_t w, uint32_t h, bool editorExtras, RenderTexture* outputRT) {
    ModuleCamera* camera = app->getCamera();
    ModuleScene* moduleScene = getActiveModuleScene();

    if (moduleScene) {
        std::function<void(GameObject*)> flush = [&](GameObject* node) {
            if (!node) return;
            if (auto* cm = node->getComponent<ComponentMesh>()) cm->flushDeferredReleases();
            for (auto* child : node->getChildren()) flush(child);
            };
        flush(moduleScene->getRoot());
    }

    const EditorSceneSettings& s = m_sceneManager->getSettings();
    const EditorSceneSettings::Skybox& sky = s.skybox;

    if (sky.enabled && m_envSystem)
        m_envSystem->render(cmd, view, proj);

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), app->getSamplerHeap()->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    m_frameLights.dirLights.clear();
    m_frameLights.pointLights.clear();
    m_frameLights.spotLights.clear();
    if (moduleScene) gatherLights(moduleScene->getRoot(), m_frameLights);

    std::vector<MeshEntry> ownedEntries;
    std::vector<MeshEntry*> visibleMeshes;

    // Skinning job list built alongside mesh collection
    std::vector<SkinningPass::SkinJob> skinJobs;
    std::vector<size_t> skinJobEntryIdx; // ownedEntries index per job
    uint32_t curPaletteOffset = 0;
    uint32_t curVertexOffset = 0;
    uint32_t curMorphWeightOffset = 0;

    if (moduleScene) {
        std::function<void(GameObject*)> collectMeshes = [&](GameObject* node) {
            if (!node || !node->isActive()) return;
            if (auto* cm = node->getComponent<ComponentMesh>()) {
                cm->flushDeferredReleases();
                Matrix nodeWorld = node->getTransform()->getGlobalMatrix();
                if (Model* model = cm->getProceduralModel()) {
                    model->buildMeshEntries(nodeWorld, ownedEntries);
                }
                else {
                    const bool isSkinned = m_skinningPass && cm->hasSkinData();

                    // Capture and clear the component-level morph dirty flag once before iterating
                    // primitives. Clearing inside the loop would prevent later primitives of the
                    // same ComponentMesh from seeing the flag on the same frame.
                    const bool morphDirtyThisFrame = m_skinningPass && cm->getMorphWeightsDirty();
                    if (morphDirtyThisFrame) cm->clearMorphWeightsDirty();

                    for (const auto& src : cm->getEntries()) {
                        // materialRes may be null when materialUID wasn't found in the sub-UID
                        // registry (e.g. simple/un-textured glTF materials).  GBufferPass
                        // handles this gracefully via instanceMaterial (always created by
                        // rebuildEntry), so only skip truly unrenderable entries.
                        if (!src.meshRes || !src.meshRes->getMesh()) continue;
                        MeshEntry e;
                        e.meshUID = src.meshUID;
                        e.materialUID = src.materialUID;
                        e.meshRes = src.meshRes;
                        e.materialRes = src.materialRes;
                        e.material = src.instanceMaterial.get();
                        e.materialCB = src.materialCB;

                        Mesh* mesh = src.meshRes->getMesh();
                        const bool hasBones = isSkinned && mesh && mesh->getBoneWeightBufferVA() != 0;

                        // Evaluate morph: active when weights are non-zero or the dirty flag was
                        // set this frame (covers the frame where weights transition back to zero).
                        bool shouldMorph = false;
                        if (m_skinningPass && mesh && mesh->hasMorphTargets()) {
                            shouldMorph = morphDirtyThisFrame;
                            if (!shouldMorph) {
                                const float* w = cm->getMorphWeights();
                                const uint32_t n = mesh->getNumMorphTargets();
                                for (uint32_t t = 0; t < n && !shouldMorph; ++t)
                                    shouldMorph = (w[t] != 0.f);
                            }
                        }

                        // Only create a GPU job when the vertex buffer is on the GPU.
                        // SkinningPass skips jobs whose vertexVA==0 but the post-dispatch loop still
                        // assigns skinnedVA, which would cause drawSkinned() to read stale data.
                        const bool vertexReady = mesh && (mesh->getVertexBufferVA() != 0);
                        const bool needsGpuJob = vertexReady && (hasBones || shouldMorph);

                        if (needsGpuJob) {
                            e.isSkinned = true; // skinnedVA filled after dispatch

                            SkinningPass::SkinJob job;
                            job.mesh = mesh;
                            job.paletteOffset = curPaletteOffset;
                            job.vertexOffset = curVertexOffset;
                            job.morphWeightOffset = curMorphWeightOffset;

                            if (hasBones) {
                                const auto& joints = cm->getSkinJoints();
                                std::vector<Matrix> jointWorlds;
                                jointWorlds.reserve(joints.size());

                                // IBP and joint global world matrices are both in glTF Y-up world space.
                                // IBP * jointGlobal cancels to Identity at T-pose — no space stripping needed.
                                int nullJointCount = 0;
                                for (auto* jgo : joints) {
                                    if (!jgo) ++nullJointCount;
                                    jointWorlds.push_back(jgo ? jgo->getTransform()->getGlobalMatrix() : Matrix::Identity);
                                }
                                if (nullJointCount > 0)
                                    LOG("[SkinDebug] WARNING: %d/%d joint GOs are null",
                                        nullJointCount, (int)joints.size());

                                job.skin = &cm->getLocalSkin();
                                job.jointWorldMatrices = std::move(jointWorlds);
                                // Skinned output is already in Y-up world space (Blender bakes the
                                // axis conversion into vertex positions at export time, and the joint
                                // global matrices include the RootNode correction). worldMatrix stays
                                // at the default Identity so no extra transform is applied.
                            } else {
                                // Morph-only: shader outputs local space; world transform in MeshEntry.
                                memcpy(e.worldMatrix, &nodeWorld, sizeof(nodeWorld));
                            }

                            if (shouldMorph) {
                                const uint32_t numTargets = mesh->getNumMorphTargets();
                                const float* w = cm->getMorphWeights();
                                job.morphWeights.assign(w, w + numTargets);
                                curMorphWeightOffset += numTargets;
                            }

                            skinJobEntryIdx.push_back(ownedEntries.size());
                            skinJobs.push_back(std::move(job));

                            if (hasBones)
                                curPaletteOffset += (uint32_t)cm->getLocalSkin().jointNodeIndices.size();
                            curVertexOffset += mesh->getVertexCount();
                        } else {
                            memcpy(e.worldMatrix, &nodeWorld, sizeof(nodeWorld));
                        }
                        ownedEntries.push_back(std::move(e));
                    }
                }
            }
            for (auto* child : node->getChildren()) collectMeshes(child);
            };
        collectMeshes(moduleScene->getRoot());
        visibleMeshes.reserve(ownedEntries.size());
        for (auto& e : ownedEntries) visibleMeshes.push_back(&e);
    }

    // Dispatch GPU skinning before the geometry pass so skinned vertex buffers are ready
    if (!skinJobs.empty() && m_skinningPass) {
        UINT frameIndex = app->getD3D12()->getCurrentBackBufferIdx();
        m_skinningPass->dispatch(cmd, skinJobs, frameIndex);

        D3D12_GPU_VIRTUAL_ADDRESS outputVA =
            m_skinningPass->getOutputBuffer(frameIndex)->GetGPUVirtualAddress();
        for (size_t i = 0; i < skinJobs.size(); ++i)
            ownedEntries[skinJobEntryIdx[i]].skinnedVA =
                outputVA + skinJobs[i].vertexOffset * sizeof(Mesh::Vertex);
    }

    const EnvironmentSystem* envForIBL =
        (sky.enabled && m_envSystem) ? m_envSystem.get() : nullptr;

    const Matrix viewProj = view * proj;

    // Split into opaque and translucent lists
    std::vector<MeshEntry*> opaqueMeshes;
    std::vector<MeshEntry*> translucentMeshes;
    opaqueMeshes.reserve(visibleMeshes.size());
    translucentMeshes.reserve(visibleMeshes.size());
    for (MeshEntry* e : visibleMeshes) {
        const Material* mat = e->instanceMaterial.get();
        if (!mat) mat = e->material;
        if (!mat && e->materialRes) mat = e->materialRes->getMaterial();
        bool isTranslucent = mat && mat->getData().baseColor.w < 0.999f;
        (isTranslucent ? translucentMeshes : opaqueMeshes).push_back(e);
    }

    // GBuffer geometry pass — fills albedo / normalMetalRough / emissiveAO / depth MRTs.
    // Always run even for translucent-only frames so depth is cleared and in DEPTH_READ state.
    if (m_gbufferPass && (!opaqueMeshes.empty() || !translucentMeshes.empty())) {
        m_gbufferPass->render(cmd, opaqueMeshes, viewProj, w, h);

        // Rebind the output render target — GBuffer pass changed OMSetRenderTargets
        if (outputRT && outputRT->isValid()) {
            auto rtv = outputRT->getRtvHandle();
            auto dsv = outputRT->getDsvHandle();
            bool hasDsv = outputRT->getDepthTexture() != nullptr;
            cmd->OMSetRenderTargets(1, &rtv, FALSE, hasDsv ? &dsv : nullptr);
            D3D12_VIEWPORT vp = { 0.f, 0.f, float(w), float(h), 0.f, 1.f };
            D3D12_RECT sc = { 0, 0, LONG(w), LONG(h) };
            cmd->RSSetViewports(1, &vp);
            cmd->RSSetScissorRects(1, &sc);
        }

        // Deferred lighting pass — fullscreen triangle reading from GBuffer SRVs
        if (m_deferredLightingPass) {
            Matrix invViewProj;
            viewProj.Invert(invViewProj);
            m_deferredLightingPass->render(cmd, *m_gbufferPass, m_frameLights,
                                            camera->getPos(), invViewProj,
                                            envForIBL, w, h);
        }

        // Transparent forward pass — sorted back-to-front, depth test only (no depth write)
        if (!translucentMeshes.empty() && m_meshRenderPass && outputRT && outputRT->isValid()) {
            // Sort furthest-first for correct alpha blending
            const Vector3 camPos = camera->getPos();
            std::sort(translucentMeshes.begin(), translucentMeshes.end(),
                      [&camPos](const MeshEntry* a, const MeshEntry* b) {
                          Matrix wa, wb;
                          memcpy(&wa, a->worldMatrix, sizeof(float) * 16);
                          memcpy(&wb, b->worldMatrix, sizeof(float) * 16);
                          float da = Vector3::DistanceSquared(wa.Translation(), camPos);
                          float db = Vector3::DistanceSquared(wb.Translation(), camPos);
                          return da > db;
                      });

            // Bind scene RT + GBuffer read-only depth (DEPTH_READ state, no writes allowed)
            auto rtv = outputRT->getRtvHandle();
            auto roDsv = m_gbufferPass->getGBuffer().getReadOnlyDsvHandle();
            cmd->OMSetRenderTargets(1, &rtv, FALSE, &roDsv);
            D3D12_VIEWPORT vp = { 0.f, 0.f, float(w), float(h), 0.f, 1.f };
            D3D12_RECT sc = { 0, 0, LONG(w), LONG(h) };
            cmd->RSSetViewports(1, &vp);
            cmd->RSSetScissorRects(1, &sc);

            BEGIN_EVENT(cmd, L"Forward Transparent Pass");
            m_meshRenderPass->renderTransparent(cmd, translucentMeshes, m_frameLights,
                                                 camPos, viewProj, envForIBL);
            END_EVENT(cmd);
        }

        // Restore descriptor heaps for subsequent passes
        ID3D12DescriptorHeap* heaps2[] = { app->getShaderDescriptors()->getHeap(),
                                           app->getSamplerHeap()->getHeap() };
        cmd->SetDescriptorHeaps(2, heaps2);
    }

    if (editorExtras) {
        if (s.showGrid) dd::xzSquareGrid(-100.f, 100.f, 0.f, 1.f, dd::colors::Gray);
        if (s.showAxis) { Matrix id = Matrix::Identity; dd::axisTriad(id.m[0], 0.f, 2.f, 2.f); }
        if (s.debugDrawLights && moduleScene) debugDrawLights(moduleScene, s.debugLightSize);

        FrustumDebugDraw fdd;
        camera->buildDebugLines(fdd);
        for (const auto& line : fdd.lines) {
            ddVec3 f = { line.from.x, line.from.y, line.from.z };
            ddVec3 t = { line.to.x, line.to.y, line.to.z };
            const Vector3& c = line.color;
            if (c.x > .5f && c.y > .5f && c.z < .5f) dd::line(f, t, dd::colors::Yellow);
            else if (c.x < .5f && c.y > .5f && c.z < .5f) dd::line(f, t, dd::colors::Green);
            else if (c.x < .5f && c.y > .5f && c.z > .5f) dd::line(f, t, dd::colors::Cyan);
            else if (c.x > .5f && c.y < .5f && c.z < .5f) dd::line(f, t, dd::colors::Red);
            else if (c.x < .5f && c.y < .5f && c.z > .5f) dd::line(f, t, dd::colors::Blue);
            else dd::line(f, t, dd::colors::White);
        }
        if (moduleScene) {
            std::function<void(GameObject*)> drawGizmos = [&](GameObject* node) {
                if (!node || !node->isActive()) return;
                for (const auto& comp : node->getComponents())
                    comp->onDrawGizmos();
                for (auto* child : node->getChildren())
                    drawGizmos(child);
            };
            drawGizmos(moduleScene->getRoot());
        }

        if (s.debugDrawBounds && moduleScene) {
            // Collect one entry per object: either a sphere or an AABB,
            // depending on whether the object has a Bounds component.
            struct BoundsEntry {
                BVType type;
                AABB   box;     // used when type == AABB
                Sphere sphere;  // used when type == Sphere
            };
            std::vector<BoundsEntry> boundsEntries;

            std::function<void(GameObject*)> collectBounds = [&](GameObject* node) {
                if (!node || !node->isActive()) return;
                if (auto* cm = node->getComponent<ComponentMesh>()) {
                    if (cm->hasAABB()) {
                        BoundsEntry e;
                        const ComponentBounds* cb = node->getComponent<ComponentBounds>();

                        if (cb && cb->bvType == BVType::Sphere) {
                            // Build sphere: center = OBB center, radius = half-diagonal
                            // (same logic as CollisionSystem::applyBVType)
                            const Matrix& W = node->getTransform()->getGlobalMatrix();
                            const Vector3 lMin = cm->getLocalAABBMin();
                            const Vector3 lMax = cm->getLocalAABBMax();
                            const Vector3 lHalf = (lMax - lMin) * 0.5f;
                            const Vector3 lCtr  = (lMin + lMax) * 0.5f;
                            Vector3 center = Vector3::Transform(lCtr, W);

                            // Extract scale magnitudes from world matrix rows
                            Vector3 cx(W._11,W._12,W._13), cy(W._21,W._22,W._23), cz(W._31,W._32,W._33);
                            float hx = lHalf.x * cx.Length();
                            float hy = lHalf.y * cy.Length();
                            float hz = lHalf.z * cz.Length();
                            float radius = (cb->radiusOverride >= 0.f)
                                ? cb->radiusOverride
                                : sqrtf(hx*hx + hy*hy + hz*hz);

                            e.type   = BVType::Sphere;
                            e.sphere = { center, radius };
                        } else {
                            Vector3 mn, mx;
                            cm->getWorldAABB(mn, mx);
                            e.type   = BVType::AABB;
                            e.box    = { mn, mx };
                        }
                        boundsEntries.push_back(e);
                    }
                }
                for (auto* child : node->getChildren()) collectBounds(child);
            };
            collectBounds(moduleScene->getRoot());

            // O(N²) pairwise overlap — handles AABB vs AABB, sphere vs sphere,
            // and sphere vs AABB mixed pairs.
            const size_t N = boundsEntries.size();
            std::vector<bool> colliding(N, false);
            for (size_t i = 0; i < N; ++i) {
                for (size_t j = i + 1; j < N; ++j) {
                    bool hit = false;
                    const BoundsEntry& ei = boundsEntries[i];
                    const BoundsEntry& ej = boundsEntries[j];
                    if (ei.type == BVType::AABB   && ej.type == BVType::AABB)
                        hit = ei.box.intersects(ej.box);
                    else if (ei.type == BVType::Sphere && ej.type == BVType::Sphere)
                        hit = ei.sphere.intersects(ej.sphere);
                    else if (ei.type == BVType::Sphere)
                        hit = ei.sphere.intersects(ej.box);
                    else
                        hit = ej.sphere.intersects(ei.box);
                    if (hit) { colliding[i] = true; colliding[j] = true; }
                }
            }

            for (size_t i = 0; i < N; ++i) {
                const float* color = colliding[i] ? dd::colors::Red : dd::colors::Green;
                const BoundsEntry& e = boundsEntries[i];
                if (e.type == BVType::Sphere)
                    dd::sphere(ddConvert(e.sphere.center), color, e.sphere.radius);
                else
                    dd::aabb(ddConvert(e.box.min), ddConvert(e.box.max), color);
            }
        }

        if (s.debugDrawGrid && m_collisionSystem)
            m_collisionSystem->drawBroadPhaseDebug();

        m_debugDraw->record(cmd, w, h, view, proj);
    }
}

void ModuleEditor::gatherLights(GameObject* node, FrameLightData& out) const {
    if (!node || !node->isActive()) return;

    if (auto* dl = node->getComponent<ComponentDirectionalLight>(); dl && dl->enabled) {
        if (out.dirLights.size() < MeshPipeline::MAX_DIR_LIGHTS) {
            MeshPipeline::GPUDirectionalLight g;
            g.direction = dl->direction;
            g.direction.Normalize();
            g.color = dl->color;
            g.intensity = dl->intensity;
            g._pad = 0.f;
            out.dirLights.push_back(g);
        }
    }

    if (auto* pl = node->getComponent<ComponentPointLight>(); pl && pl->enabled) {
        if (out.pointLights.size() < MeshPipeline::MAX_POINT_LIGHTS) {
            MeshPipeline::GPUPointLight p;
            p.position = node->getTransform()->getGlobalMatrix().Translation();
            p.squaredRadius = pl->radius * pl->radius;
            p.color = pl->color;
            p.intensity = pl->intensity;
            out.pointLights.push_back(p);
        }
    }

    if (auto* sl = node->getComponent<ComponentSpotLight>(); sl && sl->enabled) {
        if (out.spotLights.size() < MeshPipeline::MAX_SPOT_LIGHTS) {
            MeshPipeline::GPUSpotLight s;
            s.position = node->getTransform()->getGlobalMatrix().Translation();
            s.direction = sl->direction;
            s.direction.Normalize();
            s.squaredRadius = sl->radius * sl->radius;
            s.innerAngle = cosf(sl->innerAngle * kDeg2Rad);
            s.outerAngle = cosf(sl->outerAngle * kDeg2Rad);
            s.color = sl->color;
            s.intensity = sl->intensity;
            s._pad[0] = s._pad[1] = s._pad[2] = 0.f;
            out.spotLights.push_back(s);
        }
    }

    for (auto* c : node->getChildren()) gatherLights(c, out);
}

void ModuleEditor::drawDockspace() {
    constexpr ImGuiWindowFlags kF = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##Dock", nullptr, kF);
    ImGui::PopStyleVar(2);

    ImGuiID dock = ImGui::GetID("DS");
    ImGui::DockSpace(dock, ImVec2(0, 0));

    if (m_firstFrame) {
        m_firstFrame = false;
        ImGui::DockBuilderRemoveNode(dock);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dock, vp->Size);

        // Layout: left(280) | center(flex) | right(320)
        // Bottom strip is drawn as a separate overlay window, not a docked panel.
        ImGuiID left, center, rightPanel, bottom;
        float leftFrac  = 280.f / vp->Size.x;
        float rightFrac = 320.f / (vp->Size.x - 280.f);

        ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left,  leftFrac,  &left,  &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, rightFrac, &rightPanel, &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, &bottom, &center);

        // Right panel: Inspector (top ~60%) + GPU Memory (bottom ~40%)
        ImGuiID rightTop, rightBot;
        ImGui::DockBuilderSplitNode(rightPanel, ImGuiDir_Down, 0.40f, &rightBot, &rightTop);

        ImGui::DockBuilderDockWindow("Hierarchy",       left);
        ImGui::DockBuilderDockWindow("Inspector",       rightTop);
        ImGui::DockBuilderDockWindow("Scene Settings",  rightTop);
        ImGui::DockBuilderDockWindow("GPU Memory",      rightBot);
        ImGui::DockBuilderDockWindow("Scene View",      center);
        ImGui::DockBuilderDockWindow("Game View",       center);
        ImGui::DockBuilderDockWindow("Console",         bottom);
        ImGui::DockBuilderDockWindow("Asset Browser",   bottom);
        ImGui::DockBuilderDockWindow("Render Graph",    bottom);
        ImGui::DockBuilderDockWindow("Resources",       bottom);
        ImGui::DockBuilderDockWindow("Prefabs",         bottom);
        ImGui::DockBuilderFinish(dock);
    }
    ImGui::End();
}

void ModuleEditor::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    auto saveScene = [&]() {
        if (!m_currentScenePath.empty() && m_sceneManager->getActiveScene()) {
            bool ok = m_sceneManager->saveCurrentScene(m_currentScenePath);
            log(ok ? "Scene saved!" : "Failed to save.", ok ? EditorColors::Success : EditorColors::Danger);
        }
        else m_saveDialog->open(FileDialog::Type::Save, "Save Scene", "Library/Scenes");
    };

    // Brand
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Accent);
    ImGui::Text("  Phoenix  0.1.0  ");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // ---- File ----
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene",  "Ctrl+N"))        m_showNewSceneConfirm = true;
        if (ImGui::MenuItem("Open Scene", "Ctrl+O"))        m_loadDialog->open(FileDialog::Type::Open, "Load Scene", "Library/Scenes/");
        ImGui::Separator();
        if (ImGui::MenuItem("Save Scene",  "Ctrl+S"))       saveScene();
        if (ImGui::MenuItem("Save As...",  "Ctrl+Shift+S")) m_saveDialog->open(FileDialog::Type::Save, "Save Scene", "Library/Scenes/");
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")) PostQuitMessage(0);
        ImGui::EndMenu();
    }

    // ---- Edit ----
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo()))              undoToSavePoint();
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo()))              redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Copy",      "Ctrl+C", false, m_selection.has())) copySelected();
        if (ImGui::MenuItem("Paste",     "Ctrl+V", false, !m_clipboard.serialized.empty())) pasteClipboard();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, m_selection.has())) duplicateSelected();
        ImGui::Separator();
        if (ImGui::MenuItem("Editor Preferences")) {}
        ImGui::EndMenu();
    }

    // ---- GameObject ----
    if (ImGui::BeginMenu("GameObject")) {
        if (ImGui::MenuItem("Create Empty")) createEmptyGameObject();
        ImGui::Separator();
        if (ImGui::BeginMenu("Primitives")) {
            if (ImGui::MenuItem("Cube"))     spawnPrimitive(PrimitiveType::Cube);
            if (ImGui::MenuItem("Sphere"))   spawnPrimitive(PrimitiveType::Sphere);
            if (ImGui::MenuItem("Capsule"))  spawnPrimitive(PrimitiveType::Capsule);
            if (ImGui::MenuItem("Plane"))    spawnPrimitive(PrimitiveType::Plane);
            if (ImGui::MenuItem("Cylinder")) spawnPrimitive(PrimitiveType::Cylinder);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Lights")) {
            if (ImGui::MenuItem("Directional Light")) {
                auto* go = createEmptyGameObject("Directional Light");
                if (go) go->addComponent(ComponentFactory::CreateComponent(Component::Type::DirectionalLight, go));
            }
            if (ImGui::MenuItem("Point Light")) {
                auto* go = createEmptyGameObject("Point Light");
                if (go) go->addComponent(ComponentFactory::CreateComponent(Component::Type::PointLight, go));
            }
            if (ImGui::MenuItem("Spot Light")) {
                auto* go = createEmptyGameObject("Spot Light");
                if (go) go->addComponent(ComponentFactory::CreateComponent(Component::Type::SpotLight, go));
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    // ---- Component ----
    if (ImGui::BeginMenu("Component")) {
        auto addTo = [&](const char* label, Component::Type type) {
            if (ImGui::MenuItem(label, nullptr, false, m_selection.has()))
                if (m_selection.object) m_selection.object->addComponent(ComponentFactory::CreateComponent(type, m_selection.object));
        };
        addTo("Add Mesh",      Component::Type::Mesh);
        addTo("Add Rigidbody", Component::Type::Rigidbody);
        addTo("Add Camera",    Component::Type::Camera);
        addTo("Add Light (Directional)", Component::Type::DirectionalLight);
        addTo("Add Animation", Component::Type::Animation);
        ImGui::EndMenu();
    }

    // ---- Debug ----
    if (ImGui::BeginMenu("Debug")) {
        static bool s_showAABB = false, s_showFrustum = false;
        if (ImGui::MenuItem("Show AABB",    nullptr, &s_showAABB)) {
            if (m_sceneManager) m_sceneManager->getSettings().debugDrawBounds = s_showAABB;
        }
        if (ImGui::MenuItem("Show Frustum", nullptr, &s_showFrustum)) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Collision Panel")) {
            for (EditorPanel* p : m_panels)
                if (strcmp(p->getName(), "Collision Debug") == 0) { p->open = true; break; }
        }
        ImGui::EndMenu();
    }

    // ---- Window ----
    if (ImGui::BeginMenu("Window")) {
        for (EditorPanel* p : m_panels) ImGui::MenuItem(p->getName(), nullptr, &p->open);
        ImGui::EndMenu();
    }

    // ---- Transport controls (centered) ----
    {
        bool playing = m_sceneManager && m_sceneManager->isPlaying();
        bool paused  = m_sceneManager && m_sceneManager->getState() == SceneManager::PlayState::Paused;

        const float btnW  = 62.f;
        const float gap   = ImGui::GetStyle().ItemSpacing.x;
        const float stateW = 80.f;
        const float total = btnW * 3 + gap * 2 + stateW + gap;
        float centerX = (ImGui::GetContentRegionAvail().x - total) * 0.5f + ImGui::GetCursorPosX();
        if (centerX < ImGui::GetCursorPosX()) centerX = ImGui::GetCursorPosX();
        ImGui::SetCursorPosX(centerX);

        // Play
        if (playing)
            ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::toU32A(EditorColors::Ok, 0.35f));
        if (ImGui::Button("Play", ImVec2(btnW, 0)) && m_sceneManager && !playing)
            m_sceneManager->play();
        if (playing) ImGui::PopStyleColor();

        ImGui::SameLine();

        // Pause
        if (paused)
            ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::toU32A(EditorColors::Warn, 0.35f));
        if (ImGui::Button("Pause", ImVec2(btnW, 0)) && m_sceneManager && playing)
            m_sceneManager->pause();
        if (paused) ImGui::PopStyleColor();

        ImGui::SameLine();

        // Stop
        if (ImGui::Button("Stop", ImVec2(btnW, 0)) && m_sceneManager)
            stopPlay();

        // State badge
        ImGui::SameLine();
        const char* stateLabel = "EDIT";
        ImVec4      stateColor = EditorColors::Tx2;
        if (playing) { stateLabel = "PLAYING"; stateColor = EditorColors::Ok;   }
        else if (paused) { stateLabel = "PAUSED";  stateColor = EditorColors::Warn; }
        ImGui::PushStyleColor(ImGuiCol_Text, stateColor);
        ImGui::Text("%-8s", stateLabel);
        ImGui::PopStyleColor();
    }

    ImGui::EndMainMenuBar();
}

// ---------------------------------------------------------------------------
// Frame Timing Bar  (78px strip at the bottom, above the status bar)
// ---------------------------------------------------------------------------
void ModuleEditor::drawFrameTimingBar() {
    const float kBarH    = 78.f;
    const float kStatH   = 22.f; // status bar below
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float W = vp->Size.x;
    float Y = vp->Pos.y + vp->Size.y - kBarH - kStatH;

    ImGui::SetNextWindowPos({ vp->Pos.x, Y });
    ImGui::SetNextWindowSize({ W, kBarH });
    ImGui::SetNextWindowBgAlpha(1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorColors::Bg0);
    ImGui::Begin("##FrameTiming", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();

    // Pass data (ms values — eventually wired to per-pass GPU queries)
    struct PassEntry { const char* name; ImVec4 color; float ms; };
    double totalMs = m_gpuFrameTimeMs;

    // Distribute total GPU time across passes proportionally to spec ratios.
    static const PassEntry kPasses[] = {
        { "D3D12 Core",    { 0.545f,0.545f,0.588f,1.f }, 0.12f },
        { "Env/IBL",       { 0.384f,0.690f,0.788f,1.f }, 0.34f },
        { "Skinning",      { 0.435f,0.808f,0.604f,1.f }, 0.61f },
        { "G-Buffer",      { 0.910f,0.573f,0.290f,1.f }, 2.18f },
        { "Deferred Light",{ 0.910f,0.376f,0.431f,1.f }, 3.42f },
        { "Forward Mesh",  { 0.851f,0.635f,0.243f,1.f }, 1.27f },
        { "Debug Draw",    { 0.608f,0.482f,0.816f,1.f }, 0.21f },
        { "Render Texture",{ 0.353f,0.651f,0.910f,1.f }, 0.44f },
        { "ImGui",         { 0.780f,0.780f,0.812f,1.f }, 0.38f },
    };
    constexpr int kPassCount = 9;
    float passSum = 0.f;
    for (const auto& p : kPasses) passSum += p.ms;
    float scale = (passSum > 0.f && totalMs > 0.0) ? float(totalMs) / passSum : 1.f;

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      wp  = ImGui::GetWindowPos();

    // Top separator line
    dl->AddLine(wp, { wp.x + W, wp.y }, EditorColors::toU32(EditorColors::Line));

    // ---- Left column: FRAME TIME label + big number ----
    const float kLeftW = 150.f;
    {
        float cx = wp.x + 12.f, cy = wp.y + 8.f;
        dl->AddText({ cx, cy }, EditorColors::toU32(EditorColors::Tx2), "FRAME TIME");
        char msBuf[24];
        float displayMs = (totalMs > 0.0) ? float(totalMs) : passSum * scale;
        snprintf(msBuf, sizeof(msBuf), "%.2f ms", displayMs);
        dl->AddText(ImGui::GetFont(), 22.f, { cx, cy + 16.f },
                    EditorColors::toU32(EditorColors::Tx0), msBuf);
        char fpsBuf[16];
        snprintf(fpsBuf, sizeof(fpsBuf), "%.1f FPS", displayMs > 0.f ? 1000.f / displayMs : 0.f);
        dl->AddText({ cx, cy + 40.f }, EditorColors::toU32(EditorColors::Ok), fpsBuf);
    }

    // ---- Right column: draw calls, tris, VRAM ----
    const float kRightW = 130.f;
    {
        float rx = wp.x + W - kRightW - 8.f;
        float ry = wp.y + 8.f;
        dl->AddText({ rx, ry      }, EditorColors::toU32(EditorColors::Tx2), "DRAW CALLS");
        dl->AddText({ rx, ry + 16.f }, EditorColors::toU32(EditorColors::Tx0), "323");
        dl->AddText({ rx, ry + 32.f }, EditorColors::toU32(EditorColors::Tx2), "TRIS");
        dl->AddText({ rx, ry + 48.f }, EditorColors::toU32(EditorColors::Tx0), "2.64M");
    }

    // ---- Center: stacked bar + pass legend ----
    const float kBarTop  = wp.y + 8.f;
    const float kBarBotH = 26.f;
    const float kBarSegH = 14.f;
    const float barX     = wp.x + kLeftW + 8.f;
    const float barW     = W - kLeftW - kRightW - 24.f;
    const float kBudgetMs = 16.67f; // 60 fps budget

    // Budget line position
    float budgetX = barX + std::min(1.f, passSum * scale / kBudgetMs) * barW;

    // Draw segments
    float segX = barX;
    for (int i = 0; i < kPassCount; ++i) {
        float ms   = kPasses[i].ms * scale;
        float segW = (passSum * scale > 0.f) ? ms / (passSum * scale) * barW : 0.f;
        if (segW < 1.f) segW = 1.f;

        dl->AddRectFilled({ segX, kBarTop }, { segX + segW, kBarTop + kBarSegH },
                          EditorColors::toU32A(kPasses[i].color, 0.85f));
        dl->AddLine({ segX + segW, kBarTop }, { segX + segW, kBarTop + kBarSegH },
                    EditorColors::toU32(EditorColors::Bg0));

        // ms label if segment is wide enough (>5.5% of bar)
        if (segW > barW * 0.055f) {
            char lb[8]; snprintf(lb, sizeof(lb), "%.2f", ms);
            ImVec2 ts = ImGui::CalcTextSize(lb);
            if (ts.x + 4.f < segW) {
                dl->AddText({ segX + (segW - ts.x) * 0.5f, kBarTop + (kBarSegH - ts.y) * 0.5f },
                             IM_COL32(0, 0, 0, 220), lb);
            }
        }

        // Tooltip detection
        ImVec2 mpos = ImGui::GetMousePos();
        if (mpos.x >= segX && mpos.x < segX + segW &&
            mpos.y >= kBarTop && mpos.y < kBarTop + kBarSegH) {
            ImGui::SetNextWindowBgAlpha(0.85f);
            ImGui::BeginTooltip();
            ImGui::TextColored(kPasses[i].color, "%s", kPasses[i].name);
            ImGui::Text("%.2f ms  (%.1f%%)", ms, ms / (passSum * scale) * 100.f);
            ImGui::EndTooltip();
        }

        segX += segW;
    }
    dl->AddRect({ barX, kBarTop }, { barX + barW, kBarTop + kBarSegH },
                EditorColors::toU32(EditorColors::Line));

    // Budget line at 16.67ms
    if (budgetX <= barX + barW) {
        dl->AddLine({ budgetX, kBarTop - 4.f }, { budgetX, kBarTop + kBarSegH + 2.f },
                    IM_COL32(255, 80, 80, 200), 1.5f);
        dl->AddText({ budgetX + 2.f, kBarTop - 14.f },
                    IM_COL32(255, 80, 80, 200), "16.6ms");
    }

    // Pass legend row below bar
    float lx = barX;
    float ly = kBarTop + kBarSegH + 4.f;
    for (int i = 0; i < kPassCount && lx < barX + barW - 20.f; ++i) {
        float ms = kPasses[i].ms * scale;
        dl->AddRectFilled({ lx, ly + 3.f }, { lx + 8.f, ly + 11.f },
                          EditorColors::toU32(kPasses[i].color), 1.f);
        lx += 10.f;
        char lb[32]; snprintf(lb, sizeof(lb), "%s %.2f", kPasses[i].name, ms);
        ImVec2 ts = ImGui::CalcTextSize(lb);
        dl->AddText({ lx, ly }, EditorColors::toU32(EditorColors::Tx2), lb);
        lx += ts.x + 8.f;
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Status Bar (22px strip at the very bottom)
// ---------------------------------------------------------------------------
void ModuleEditor::drawStatusBar() {
    const float kStatH = 22.f;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float W = vp->Size.x;
    float Y = vp->Pos.y + vp->Size.y - kStatH;

    ImGui::SetNextWindowPos({ vp->Pos.x, Y });
    ImGui::SetNextWindowSize({ W, kStatH });
    ImGui::SetNextWindowBgAlpha(1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 3.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorColors::Bg0);
    ImGui::Begin("##StatusBar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    dl->AddLine(wp, { wp.x + W, wp.y }, EditorColors::toU32(EditorColors::Line));

    // Left side: scene info
    {
        std::string sceneName = m_currentScenePath.empty() ? "Untitled_01.scene"
            : fs::path(m_currentScenePath).filename().string();

        int goCount = 0, compCount = 0;
        if (auto* scene = getActiveModuleScene()) {
            std::function<void(GameObject*)> count = [&](GameObject* n) {
                if (!n) return; ++goCount;
                compCount += (int)n->getComponents().size();
                for (auto* c : n->getChildren()) count(c);
            };
            count(scene->getRoot());
        }

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
        ImGui::Text("%s  |  %d GameObjects  |  %d Components",
                    sceneName.c_str(), goCount, compCount);
        ImGui::PopStyleColor();
    }

    // Right side: GPU adapter + engine version
    {
        static std::string s_adapterName;
        if (s_adapterName.empty()) {
            if (auto* dev = app->getD3D12()->getDevice()) {
                ComPtr<IDXGIDevice>  dxgiDev;
                ComPtr<IDXGIAdapter> adapter;
                if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&dxgiDev))) && dxgiDev)
                    if (SUCCEEDED(dxgiDev->GetAdapter(&adapter)) && adapter) {
                        DXGI_ADAPTER_DESC desc = {};
                        adapter->GetDesc(&desc);
                        // Convert wide to narrow
                        char narrow[128] = {};
                        WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, narrow, sizeof(narrow) - 1, nullptr, nullptr);
                        s_adapterName = narrow;
                    }
            }
            if (s_adapterName.empty()) s_adapterName = "Unknown GPU";
        }

        char rhs[128];
        snprintf(rhs, sizeof(rhs), "%s  |  D3D12_2  |  Phoenix Engine v0.1.0",
                 s_adapterName.c_str());

        ImVec2 ts = ImGui::CalcTextSize(rhs);
        float rx = W - ts.x - 12.f;

        ImDrawList* wdl = ImGui::GetWindowDrawList();
        ImVec2 rhsPos = { wp.x + rx, wp.y + (kStatH - ts.y) * 0.5f };

        // Render most of the string in Tx1, last part (Phoenix Engine) in Accent
        const char* accentStart = strstr(rhs, "Phoenix Engine");
        if (accentStart) {
            std::string part1(rhs, static_cast<size_t>(accentStart - rhs));
            wdl->AddText(rhsPos, EditorColors::toU32(EditorColors::Tx1), part1.c_str());
            ImVec2 ts1 = ImGui::CalcTextSize(part1.c_str());
            wdl->AddText({ rhsPos.x + ts1.x, rhsPos.y },
                         EditorColors::toU32(EditorColors::Accent), accentStart);
        }
        else {
            wdl->AddText(rhsPos, EditorColors::toU32(EditorColors::Tx1), rhs);
        }
    }

    ImGui::End();
}

void ModuleEditor::handleDialogs() {
    auto tryScene = [&](bool ok, const char* good, const char* bad) { log(ok ? good : bad, ok ? EditorColors::Success : EditorColors::Danger); };
    if (m_saveDialog->draw() && m_sceneManager->getActiveScene()) {
        const std::string& p = m_saveDialog->getSelectedPath();
        if (m_sceneManager->saveCurrentScene(p)) {
            m_currentScenePath = p;
            m_savePointIndex = (int)m_undoStack.size();
            m_redoStack.clear();
            tryScene(true, "Scene saved!", "");
        }
        else tryScene(false, "", "Failed to save scene.");
    }
    if (m_loadDialog->draw() && m_sceneManager->getActiveScene()) {
        const std::string& p = m_loadDialog->getSelectedPath();
        if (m_sceneManager->loadScene(p)) { m_currentScenePath = p; tryScene(true, "Scene loaded!", ""); }
        else tryScene(false, "", "Failed to load scene.");
    }
}

void ModuleEditor::handleNewScenePopup(ID3D12GraphicsCommandList*) {
    if (m_showNewSceneConfirm) { ImGui::OpenPopup("New Scene?"); m_showNewSceneConfirm = false; }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal("New Scene?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::Text("This will clear the current scene.");
    ImGui::TextColored(EditorColors::Warning, "Unsaved changes will be lost!");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    if (ImGui::Button("Create New Scene", ImVec2(160, 0))) {
        app->getD3D12()->flush();
        m_sceneManager->setScene(std::make_unique<EmptyScene>(), app->getD3D12()->getDevice());
        m_selection.clear();
        m_currentScenePath.clear();
        m_undoStack.clear();
        m_redoStack.clear();
        m_savePointIndex = 0;
        log("New scene created.", EditorColors::Success);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

GameObject* ModuleEditor::createEmptyGameObject(const char* name, GameObject* parent) {
    ModuleScene* scene = getActiveModuleScene();
    if (!scene) return nullptr;
    GameObject* go = scene->createGameObject(name);
    if (parent) go->setParent(parent);
    m_selection.object = go;
    log(("Created: " + std::string(name)).c_str(), EditorColors::Success);

    std::string goName = go->getName();
    auto serialized = std::make_shared<std::string>();
    auto livePtr = std::make_shared<GameObject*>(go);

    pushCommand({
        [this, serialized, livePtr, goName]() {
            ModuleScene* s = getActiveModuleScene();
            if (!s) return;
            GameObject* restored = serialized->empty()
                ? s->createGameObject(goName.c_str())
                : PrefabManager::deserializeGameObject(*serialized, s);
            if (restored) { *livePtr = restored; m_selection.object = restored; log(("Redo create: " + goName).c_str(), EditorColors::Success); }
        },
        [this, livePtr, serialized, goName]() {
            GameObject* go = *livePtr;
            if (!go) return;
            *serialized = PrefabManager::serializeGameObject(go);
            if (m_selection.object == go) m_selection.clear();
            app->getD3D12()->flush();
            ModuleScene* s = getActiveModuleScene();
            if (s) s->destroyGameObject(go);
            *livePtr = nullptr;
            log(("Undo create: " + goName).c_str(), EditorColors::Warning);
        }
        });

    return go;
}

void ModuleEditor::deleteGameObject(GameObject* go) {
    if (!go) return;
    if (m_selection.object == go || isChildOf(go, m_selection.object)) m_selection.clear();
    ModuleScene* scene = getActiveModuleScene();
    if (!scene) return;
    GameObject* par = go->getParent();
    for (auto* c : go->getChildren()) c->setParent(par);
    std::string name = go->getName();
    std::string serialized = PrefabManager::serializeGameObject(go);

    app->getD3D12()->flush();
    scene->destroyGameObject(go);
    log(("Deleted: " + name).c_str(), EditorColors::Warning);

    auto livePtr = std::make_shared<GameObject*>(nullptr);

    pushCommand({
        [this, livePtr, serialized, name]() {
            GameObject* target = *livePtr;
            if (!target) return;
            if (m_selection.object == target) m_selection.clear();
            app->getD3D12()->flush();
            ModuleScene* s = getActiveModuleScene();
            if (s) s->destroyGameObject(target);
            *livePtr = nullptr;
            log(("Redo delete: " + name).c_str(), EditorColors::Warning);
        },
        [this, livePtr, serialized, name]() {
            ModuleScene* s = getActiveModuleScene();
            if (!s) return;
            GameObject* restored = PrefabManager::deserializeGameObject(serialized, s);
            if (restored) {
                *livePtr = restored;
                m_selection.object = restored;
                log(("Undo delete: " + name).c_str(), EditorColors::Success);
            }
        }
        });
}

void ModuleEditor::spawnAssetAtPath(const std::string& path) {
    if (path.empty() || !fs::exists(path) || fs::is_directory(path)) return;
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    ModuleScene* scene = getActiveModuleScene();
    if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj") {
        if (!scene) return;
        if (GameObject* go = spawnModel(path)) m_selection.object = go;
    }
    else if (ext == ".json") {
        if (m_sceneManager && m_sceneManager->loadScene(path)) log(("Loaded scene: " + path).c_str(), EditorColors::Success);
    }
    else if (ext == ".prefab") {
        if (!scene) return;
        std::string stem = fs::path(path).stem().string();
        PrefabManager::instantiatePrefab(stem, scene);
        log(("Instantiated: " + stem).c_str(), EditorColors::Success);
    }
}

GameObject* ModuleEditor::spawnModel(const std::string& path) {
    ModuleScene* scene = getActiveModuleScene();
    if (!scene) return nullptr;

    std::string stem = fs::path(path).stem().string();

    UID uid = app->getAssets()->findUID(path);
    if (uid == 0) {
        log(("Cannot find UID for: " + path).c_str(), EditorColors::Danger);
        return nullptr;
    }

    // Use the hierarchy system when node metadata is available and the model has
    // multiple mesh nodes OR skin data (skinned models need the full node hierarchy
    // even if they only have one mesh node).  Fall back to the simple single-GameObject
    // path for older imports without nodes.meta or non-GLTF formats.
    ResourceModel* model = app->getResources()->RequestModel(uid);
    if (model) {
        int meshNodeCount = 0;
        for (const auto& n : model->getNodes())
            if (!n.meshes.empty()) ++meshNodeCount;

        bool hasAnimations = (app->getAssets()->findSubUID(path, "anim", 0) != 0);
        bool hasSkin = !model->getSkins().empty();
        bool needsHierarchy = meshNodeCount > 1 || hasSkin || hasAnimations;
        if (needsHierarchy) {
            GameObject* root = model->spawnIntoScene(scene);
            app->getResources()->ReleaseResource(model);
            if (root) {
                bool animComp = root->getComponent<ComponentAnimation>() != nullptr;
                LOG("spawnModel '%s': meshNodes=%d skin=%s anim=%s AnimComponent=%s",
                    stem.c_str(), meshNodeCount,
                    hasSkin ? "yes" : "no",
                    hasAnimations ? "yes" : "no",
                    animComp ? "yes" : "no");
                log(("Added: " + stem).c_str(), EditorColors::Success);
                return root;
            }
        } else {
            app->getResources()->ReleaseResource(model);
        }
    }

    // Fallback: no node metadata, single-mesh model, or non-GLTF format
    GameObject* go = scene->createGameObject(stem);
    bool ok = go->createComponent<ComponentMesh>()->loadModel(path.c_str());
    log(ok ? ("Added: " + stem).c_str() : ("Failed: " + path).c_str(),
        ok ? EditorColors::Success : EditorColors::Danger);
    return ok ? go : nullptr;
}

bool ModuleEditor::isChildOf(const GameObject* root, const GameObject* needle) {
    if (!root || !needle) return false;
    if (root == needle) return true;
    for (const auto* c : root->getChildren()) if (isChildOf(c, needle)) return true;
    return false;
}

void ModuleEditor::updateMemory() {
    uint64_t gpuMB = 0, ramMB = 0;
    if (ID3D12Device* device = app->getD3D12()->getDevice()) {
        ComPtr<IDXGIDevice> dxgiDev;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIAdapter3> adapter3;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgiDev))) && dxgiDev)
            if (SUCCEEDED(dxgiDev->GetAdapter(&adapter)) && adapter)
                if (SUCCEEDED(adapter.As(&adapter3)) && adapter3) {
                    DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
                    if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
                        gpuMB = info.CurrentUsage / (1024 * 1024);
                }
    }
    MEMORYSTATUSEX mem = { sizeof(mem) };
    GlobalMemoryStatusEx(&mem);
    ramMB = (mem.ullTotalPhys - mem.ullAvailPhys) / (1024 * 1024);
    m_performance->setMemory(gpuMB, ramMB);
}

void ModuleEditor::debugDrawLights(ModuleScene* scene, float sz) {
    if (!scene) return;
    auto v = [](const Vector3& x) -> const float* { return &x.x; };
    std::function<void(GameObject*)> visit = [&](GameObject* node) {
        if (!node || !node->isActive()) return;
        if (auto* dl = node->getComponent<ComponentDirectionalLight>(); dl && dl->enabled) {
            Vector3 p = node->getTransform()->getGlobalMatrix().Translation();
            Vector3 d = dl->direction; d.Normalize();
            float h = sz * .2f;
            dd::line(v(p), v(p + d * sz * 2.f), dd::colors::Yellow);
            dd::line(v(p - Vector3(h, 0, 0)), v(p + Vector3(h, 0, 0)), dd::colors::Yellow);
            dd::line(v(p - Vector3(0, h, 0)), v(p + Vector3(0, h, 0)), dd::colors::Yellow);
            dd::line(v(p - Vector3(0, 0, h)), v(p + Vector3(0, 0, h)), dd::colors::Yellow);
        }
        if (auto* pl = node->getComponent<ComponentPointLight>(); pl && pl->enabled) {
            Vector3 p = node->getTransform()->getGlobalMatrix().Translation();
            float h = sz * .2f;
            dd::sphere(v(p), dd::colors::Cyan, pl->radius);
            dd::line(v(p - Vector3(h, 0, 0)), v(p + Vector3(h, 0, 0)), dd::colors::Cyan);
            dd::line(v(p - Vector3(0, h, 0)), v(p + Vector3(0, h, 0)), dd::colors::Cyan);
            dd::line(v(p - Vector3(0, 0, h)), v(p + Vector3(0, 0, h)), dd::colors::Cyan);
        }
        if (auto* sl = node->getComponent<ComponentSpotLight>(); sl && sl->enabled) {
            Vector3 p = node->getTransform()->getGlobalMatrix().Translation();
            Vector3 dir = sl->direction; dir.Normalize();
            float outerR = tanf(sl->outerAngle * kDeg2Rad) * sl->radius;
            Vector3 tip = p + dir * sl->radius;
            Vector3 up = (fabsf(dir.y) < .99f) ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
            Vector3 right = dir.Cross(up); right.Normalize();
            up = right.Cross(dir); up.Normalize();
            const int segs = 8;
            for (int i = 0; i < segs; ++i) {
                float a0 = float(i) / segs * 6.28318530f;
                float a1 = float(i + 1) / segs * 6.28318530f;
                Vector3 o0 = tip + (right * cosf(a0) + up * sinf(a0)) * outerR;
                Vector3 o1 = tip + (right * cosf(a1) + up * sinf(a1)) * outerR;
                dd::line(v(p), v(o0), dd::colors::Orange);
                dd::line(v(o0), v(o1), dd::colors::Orange);
            }
            float h = sz * .2f;
            dd::line(v(p - Vector3(h, 0, 0)), v(p + Vector3(h, 0, 0)), dd::colors::Orange);
            dd::line(v(p - Vector3(0, h, 0)), v(p + Vector3(0, h, 0)), dd::colors::Orange);
            dd::line(v(p - Vector3(0, 0, h)), v(p + Vector3(0, 0, h)), dd::colors::Orange);
        }
        for (auto* c : node->getChildren()) visit(c);
        };
    visit(scene->getRoot());
}

ImVec2 ModuleEditor::getSceneViewSize() const {
    return m_sceneView ? m_sceneView->viewport.size : ImVec2(0, 0);
}

void ModuleEditor::pushCommand(EditorCommand cmd) {
    m_redoStack.clear();
    m_undoStack.push_back(std::move(cmd));
    if ((int)m_undoStack.size() > kMaxUndoSteps) {
        m_undoStack.pop_front();
        if (m_savePointIndex > 0) --m_savePointIndex;
    }
}

bool ModuleEditor::canUndo() const { return (int)m_undoStack.size() > m_savePointIndex; }
bool ModuleEditor::canRedo() const { return !m_redoStack.empty(); }

void ModuleEditor::undoToSavePoint() {
    if (!canUndo()) return;
    EditorCommand& cmd = m_undoStack.back();
    cmd.undo();
    m_redoStack.push_back(std::move(cmd));
    m_undoStack.pop_back();
}

void ModuleEditor::redo() {
    if (!canRedo()) return;
    EditorCommand& cmd = m_redoStack.back();
    cmd.execute();
    m_undoStack.push_back(std::move(cmd));
    m_redoStack.pop_back();
}

void ModuleEditor::copySelected() {
    if (!m_selection.has()) return;
    m_clipboard.name = m_selection.object->getName();
    m_clipboard.serialized = PrefabManager::serializeGameObject(m_selection.object);
    log(("Copied: " + m_clipboard.name).c_str(), EditorColors::Info);
}

void ModuleEditor::pasteClipboard() {
    if (m_clipboard.serialized.empty()) return;
    ModuleScene* scene = getActiveModuleScene();
    if (!scene) return;
    GameObject* pasted = PrefabManager::deserializeGameObject(m_clipboard.serialized, scene);
    if (!pasted) return;
    std::string pastedName = pasted->getName();
    m_selection.object = pasted;
    log(("Pasted: " + pastedName).c_str(), EditorColors::Success);

    std::string clipData = m_clipboard.serialized;
    auto livePtr = std::make_shared<GameObject*>(pasted);

    pushCommand({
        [this, clipData, livePtr, pastedName]() {
            ModuleScene* s = getActiveModuleScene();
            if (!s) return;
            GameObject* restored = PrefabManager::deserializeGameObject(clipData, s);
            if (restored) { *livePtr = restored; m_selection.object = restored; log(("Redo paste: " + pastedName).c_str(), EditorColors::Success); }
        },
        [this, livePtr, pastedName]() {
            GameObject* p = *livePtr;
            if (!p) return;
            if (m_selection.object == p) m_selection.clear();
            app->getD3D12()->flush();
            ModuleScene* s = getActiveModuleScene();
            if (s) s->destroyGameObject(p);
            *livePtr = nullptr;
            log(("Undo paste: " + pastedName).c_str(), EditorColors::Warning);
        }
        });
}

void ModuleEditor::duplicateSelected() {
    if (!m_selection.has()) return;
    std::string serialized = PrefabManager::serializeGameObject(m_selection.object);
    std::string srcName = m_selection.object->getName();
    ModuleScene* scene = getActiveModuleScene();
    if (!scene) return;
    GameObject* dupe = PrefabManager::deserializeGameObject(serialized, scene);
    if (!dupe) return;
    std::string dupeName = dupe->getName();
    m_selection.object = dupe;
    log(("Duplicated: " + dupeName).c_str(), EditorColors::Success);

    auto livePtr = std::make_shared<GameObject*>(dupe);

    pushCommand({
        [this, serialized, livePtr, dupeName]() {
            ModuleScene* s = getActiveModuleScene();
            if (!s) return;
            GameObject* restored = PrefabManager::deserializeGameObject(serialized, s);
            if (restored) { *livePtr = restored; m_selection.object = restored; log(("Redo duplicate: " + dupeName).c_str(), EditorColors::Success); }
        },
        [this, livePtr, dupeName]() {
            GameObject* d = *livePtr;
            if (!d) return;
            if (m_selection.object == d) m_selection.clear();
            app->getD3D12()->flush();
            ModuleScene* s = getActiveModuleScene();
            if (s) s->destroyGameObject(d);
            *livePtr = nullptr;
            log(("Undo duplicate: " + dupeName).c_str(), EditorColors::Warning);
        }
        });
}

// ---------------------------------------------------------------------------
// spawnPrimitive — create a procedural mesh object in the active scene
// ---------------------------------------------------------------------------
GameObject* ModuleEditor::spawnPrimitive(PrimitiveType type,
                                          const Vector3& position,
                                          const Vector3& scale,
                                          bool           addPhysics)
{
    ModuleScene* scene = getActiveModuleScene();
    if (!scene) return nullptr;

    static const char* kNames[] = { "Cube","Sphere","Capsule","Plane","Cylinder" };
    int ti = static_cast<int>(type);
    const char* name = (ti >= 0 && ti < 5) ? kNames[ti] : "Primitive";

    std::unique_ptr<Mesh> mesh;
    switch (type) {
    case PrimitiveType::Cube:     mesh = PrimitiveFactory::createCubeMesh();     break;
    case PrimitiveType::Sphere:   mesh = PrimitiveFactory::createSphereMesh();   break;
    case PrimitiveType::Capsule:  mesh = PrimitiveFactory::createCapsuleMesh();  break;
    case PrimitiveType::Plane:    mesh = PrimitiveFactory::createPlaneMesh();    break;
    case PrimitiveType::Cylinder: mesh = PrimitiveFactory::createCylinderMesh(); break;
    default: return nullptr;
    }

    GameObject* go = scene->createGameObject(name);
    auto* t = go->getTransform();
    t->position = position;
    t->scale    = scale;
    t->markDirty();

    auto* cm = go->createComponent<ComponentMesh>();
    cm->setProceduralModel(PrimitiveFactory::meshToModel(std::move(mesh)));

    // Sphere mesh → sphere bounding volume gives a tighter collision fit.
    if (type == PrimitiveType::Sphere) {
        auto* cb = go->createComponent<ComponentBounds>();
        cb->bvType = BVType::Sphere;
    }

    if (addPhysics) {
        auto* rb = go->createComponent<ComponentRigidbody>();
        rb->mass          = 1.f;
        rb->useGravity    = true;
        rb->restitution   = 0.5f;
        rb->linearDamping = 0.1f;
    }

    m_selection.object = go;
    log(std::string("Spawned ").append(name).c_str(), EditorColors::Success);
    return go;
}

// ---------------------------------------------------------------------------
// stopPlay — safe stop: restores scene then clears pointers that would dangle
// ---------------------------------------------------------------------------
void ModuleEditor::stopPlay() {
    if (m_sceneManager) m_sceneManager->stop();
    // The scene was just restored from the temp save; every raw pointer the
    // editor held is now dangling.  Clear them before the next frame.
    m_selection.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_savePointIndex = 0;
}

// ---------------------------------------------------------------------------

void ModuleEditor::handleShortcuts() {
    if (ImGui::GetIO().WantTextInput) return;
    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl;
    bool shift = io.KeyShift;
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_N, false)) m_showNewSceneConfirm = true;
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_N, false)) createEmptyGameObject();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        if (!m_currentScenePath.empty() && m_sceneManager->getActiveScene()) {
            bool ok = m_sceneManager->saveCurrentScene(m_currentScenePath);
            if (ok) { m_savePointIndex = (int)m_undoStack.size(); m_redoStack.clear(); }
            log(ok ? "Scene saved!" : "Failed to save.", ok ? EditorColors::Success : EditorColors::Danger);
        }
        else m_saveDialog->open(FileDialog::Type::Save, "Save Scene", "Library/Scenes");
    }
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_S, false)) m_saveDialog->open(FileDialog::Type::Save, "Save Scene", "Library/Scenes/");
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) m_loadDialog->open(FileDialog::Type::Open, "Load Scene", "Library/Scenes/");
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && m_selection.has()) deleteGameObject(m_selection.object);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && m_sceneManager && m_sceneManager->isEditingPrefab()) { exitPrefabEdit(); return; }
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) undoToSavePoint();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Y, false)) redo();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_C, false)) copySelected();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_V, false)) pasteClipboard();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_D, false)) duplicateSelected();

    // Shift+P — spawn a random primitive above the scene floor with physics.
    // Cycles through Cube/Sphere/Capsule/Cylinder so you can press it many times
    // to fill the scene for collision testing.
    if (!ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_P, false)) {
        static int spawnIdx = 0;
        ++spawnIdx;
        static const PrimitiveType kTypes[] = {
            PrimitiveType::Cube, PrimitiveType::Sphere,
            PrimitiveType::Capsule, PrimitiveType::Cylinder
        };
        PrimitiveType t = kTypes[spawnIdx % 4];
        float x = static_cast<float>((spawnIdx * 3) % 11 - 5);
        float z = static_cast<float>((spawnIdx * 7) % 11 - 5);
        spawnPrimitive(t, Vector3(x, 5.f, z), Vector3::One, true);
    }
}

void ModuleEditor::enterPrefabEdit(const std::string& prefabName) {
    if (!m_sceneManager) return;
    app->getD3D12()->flush();
    if (m_sceneManager->isEditingPrefab()) m_sceneManager->exitPrefabEdit();
    m_prefabSession.clear();
    m_prefabSession.isolatedScene = std::make_unique<ModuleScene>();
    GameObject* loaded = PrefabManager::instantiatePrefab(prefabName, m_prefabSession.isolatedScene.get());
    if (!loaded) {
        log(("Failed to open prefab: " + prefabName).c_str(), EditorColors::Danger);
        m_prefabSession.clear();
        return;
    }
    m_prefabSession.prefabName = prefabName;
    m_prefabSession.rootObject = loaded;
    m_prefabSession.active = true;
    m_selection.object = loaded;
    m_sceneManager->enterPrefabEdit(m_prefabSession.isolatedScene.get(), prefabName);
    log(("Editing prefab: " + prefabName).c_str(), EditorColors::Active);
}

void ModuleEditor::exitPrefabEdit() {
    if (!m_sceneManager || !m_sceneManager->isEditingPrefab()) return;
    m_pendingExitPrefab = true;
}

void ModuleEditor::flushExitPrefabEdit() {
    if (!m_pendingExitPrefab) return;
    m_pendingExitPrefab = false;
    app->getD3D12()->flush();
    m_selection.clear();
    m_sceneManager->exitPrefabEdit();
    m_prefabSession.clear();
    log("Exited prefab edit.", EditorColors::Muted);
}

void ModuleEditor::onScriptFileEvent(const std::string& absPath, FileWatcher::Event ev) {
    if (absPath.size() < 4) return;
    std::string ext = absPath.substr(absPath.size() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".dll") return;

    switch (ev) {
    case FileWatcher::Event::Added:
        log("[ScriptWatch] New DLL: %s", EditorColors::Success);
        m_hotReload->loadLibrary(absPath);
        break;
    case FileWatcher::Event::Modified:
        log("[ScriptWatch] DLL changed: %s — reloading", EditorColors::White);
        m_hotReload->reloadLibrary(absPath);
        break;
    case FileWatcher::Event::Deleted:
        log("[ScriptWatch] DLL removed: %s", EditorColors::Danger);
        break;
    }
}

void ModuleEditor::notifyScriptComponentsReload(const std::string& /*dllPath*/) {
    auto* scene = getActiveModuleScene();
    if (!scene) return;

    std::function<void(GameObject*)> visit = [&](GameObject* node) {
        if (!node) return;
        for (const auto& comp : node->getComponents()) {
            if (comp->getType() == Component::Type::Script) {
                auto* sc = static_cast<ComponentScript*>(comp.get());
                sc->onDllReloaded(m_hotReload.get());
            }
        }
        for (auto* child : node->getChildren())
            visit(child);
        };
    visit(scene->getRoot());
}

// ---------------------------------------------------------------------------
// Drag-drop overlay
// ---------------------------------------------------------------------------
static void drawDashedRect(ImDrawList* dl, ImVec2 a, ImVec2 b,
                            ImU32 col, float thick, float dash, float gap){
    auto seg = [&](ImVec2 p0, ImVec2 p1) {
        float dx = p1.x - p0.x, dy = p1.y - p0.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.001f) return;
        float nx = dx / len, ny = dy / len, t = 0.f;
        bool on = true;
        while (t < len) {
            float end = (std::min)(t + (on ? dash : gap), len);
            if (on)
                dl->AddLine({ p0.x + nx * t, p0.y + ny * t },
                             { p0.x + nx * end, p0.y + ny * end },
                             col, thick);
            t = end;
            on = !on;
        }
    };
    seg({ a.x, a.y }, { b.x, a.y });
    seg({ b.x, a.y }, { b.x, b.y });
    seg({ b.x, b.y }, { a.x, b.y });
    seg({ a.x, b.y }, { a.x, a.y });
}

void ModuleEditor::drawDragDropOverlay(){
    auto& ddm = DragDropManager::Get();
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    const ImVec2 dsz = ImGui::GetIO().DisplaySize;

    // ---- Drag-hover overlay -------------------------------------------------
    if (ddm.IsDragging()) {
        // Semi-transparent full-screen dim (~40 % opacity)
        fg->AddRectFilled({ 0.f, 0.f }, dsz, IM_COL32(0, 0, 0, 100));

        // Centred drop box
        constexpr float bW = 440.f, bH = 130.f;
        const ImVec2 bMin{ dsz.x * 0.5f - bW * 0.5f, dsz.y * 0.5f - bH * 0.5f };
        const ImVec2 bMax{ dsz.x * 0.5f + bW * 0.5f, dsz.y * 0.5f + bH * 0.5f };

        fg->AddRectFilled(bMin, bMax, IM_COL32(18, 20, 30, 225), 10.f);
        drawDashedRect(fg, bMin, bMax, IM_COL32(80, 160, 255, 220), 2.f, 14.f, 6.f);

        const char* kLine1 = "Drop Assets to Import";
        const char* kLine2 = ".gltf  .glb  .fbx  .obj  .png  .jpg  .tga  .dds  .hdr";
        const ImVec2 s1 = ImGui::CalcTextSize(kLine1);
        const ImVec2 s2 = ImGui::CalcTextSize(kLine2);
        const float cx = (bMin.x + bMax.x) * 0.5f;
        const float cy = (bMin.y + bMax.y) * 0.5f;

        fg->AddText({ cx - s1.x * 0.5f, cy - s1.y - 6.f },
                    IM_COL32(210, 225, 255, 255), kLine1);
        fg->AddText({ cx - s2.x * 0.5f, cy + 6.f },
                    IM_COL32(120, 145, 185, 200), kLine2);
    }

    // ---- Import progress modal (centered, 500x200) --------------------------
    // Sits on top of the same semi-transparent backdrop drawn by the hover
    // section above; no second dim layer is added here.
    const DragDropManager::ImportProgress prog = ddm.GetProgress();
    if (prog.active || prog.showComplete) {
        constexpr float kW = 500.f, kH = 200.f;
        ImGui::SetNextWindowPos(
            { dsz.x * 0.5f - kW * 0.5f, dsz.y * 0.5f - kH * 0.5f },
            ImGuiCond_Always);
        ImGui::SetNextWindowSize({ kW, kH }, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.93f);

        constexpr ImGuiWindowFlags kFlags =
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoInputs;

        if (ImGui::Begin("##DDProgressModal", nullptr, kFlags)) {
            if (prog.active) {
                // Large bold filename (truncated to 60 chars with leading "…")
                std::string fname = prog.currentFile;
                if (fname.size() > 60)
                    fname = "\xe2\x80\xa6" + fname.substr(fname.size() - 59);

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.92f, 1.0f, 1.f));
                ImGui::SetWindowFontScale(1.25f);
                ImGui::TextUnformatted(fname.c_str());
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();

                ImGui::Spacing();

                char subtitle[64];
                snprintf(subtitle, sizeof(subtitle),
                         "Importing %d of %d file%s",
                         prog.current + 1, prog.total,
                         prog.total == 1 ? "" : "s");
                ImGui::TextColored({ 0.65f, 0.72f, 0.82f, 1.f }, "%s", subtitle);

                ImGui::Spacing();

                const float pct = (prog.total > 0)
                    ? static_cast<float>(prog.current) / static_cast<float>(prog.total)
                    : 0.f;
                ImGui::ProgressBar(pct, { -1.f, 0.f });

            } else {
                // showComplete banner
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.92f, 0.45f, 1.f));
                ImGui::SetWindowFontScale(1.25f);
                char done[80];
                snprintf(done, sizeof(done), "Import complete  (%d file%s)",
                         prog.total, prog.total == 1 ? "" : "s");
                ImGui::TextUnformatted(done);
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::ProgressBar(1.f, { -1.f, 0.f });
            }

            // Scrollable list of the last 6 completed files
            if (!prog.completedFiles.empty()) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::BeginChild("##DDDoneList", { 0.f, 0.f }, false,
                                  ImGuiWindowFlags_NoScrollbar);
                for (const auto& cf : prog.completedFiles) {
                    ImGui::TextColored({ 0.45f, 0.88f, 0.52f, 1.f },
                                       "\xe2\x9c\x93 %s", cf.c_str());
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }
}
