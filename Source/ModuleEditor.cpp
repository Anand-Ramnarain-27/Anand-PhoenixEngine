#include "Globals.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ImGuiPass.h"
#include "DebugDrawPass.h"
#include "MeshRenderPass.h"
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
#include "EnvironmentMap.h"
#include "SceneViewPanel.h"
#include "GameViewPanel.h"
#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "AssetBrowserPanel.h"
#include "SceneSettingsPanel.h"
#include "PrefabManager.h"
#include "FileWatcher.h"
#include "Model.h"
#include "MeshEntry.h"
#include "ResourceSkin.h"
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
    m_sceneManager = std::make_unique<SceneManager>();
    m_meshRenderPass = std::make_unique<MeshRenderPass>();
    m_hotReload = std::make_unique<HotReloadManager>();
    m_skinningPass = std::make_unique<SkinningPass>();

    if (!m_skinningPass->init(device)) {
        return false;
    }

    m_hotReload->setReloadCallback([this](const std::string& dllPath) {
        notifyScriptComponentsReload(dllPath);
        });

    std::string scriptDir = app->getFileSystem()->GetAssetsPath() + std::string("Scripts/");
    app->getFileSystem()->CreateDir(scriptDir.c_str());

    m_scriptWatcher.start(scriptDir,
        [this](const std::string& absPath, FileWatcher::Event ev) {
            onScriptFileEvent(absPath, ev);
        });

    auto existing = app->getFileSystem()->GetFilesInDirectory(scriptDir.c_str(), ".dll");
    for (const auto& path : existing)
        m_hotReload->loadLibrary(path);

    if (!m_meshRenderPass->init(device)) return false;

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
    addPanel<AssetBrowserPanel>(this);
    addPanel<SceneSettingsPanel>(this);
    addPanel<ResourcesPanel>(this);

    log("[Editor] Initialized", EditorColors::Success);
    return true;
}

bool ModuleEditor::cleanUp() {
    m_ownedPanels.clear();
    m_panels.clear();
    m_envSystem.reset();
    m_imguiPass.reset();
    m_debugDraw.reset();
    m_sceneManager.reset();
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
    if (m_sceneManager) m_sceneManager->update(app->getElapsedMilis() * 0.001f);
    m_performance->pushFPS(app->getFPS());
    drawDockspace();
    drawMenuBar();
    for (EditorPanel* p : m_panels) if (p->open) p->draw();
    handleDialogs();
}

void ModuleEditor::render() {
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleShaderDescriptors* descs = app->getShaderDescriptors();
    ID3D12GraphicsCommandList* cmd = d3d12->getCommandList();

    m_scriptWatcher.poll();

    m_frameTransientBuffers.clear();

    cmd->Reset(d3d12->getCommandAllocator(), nullptr);

    if (m_sceneView->viewport.isReady()) {
        ModuleScene* ms = getActiveModuleScene();
        if (ms) {
            std::vector<SkinInstance> skinInstances;
            collectSkinInstances(ms->getRoot(), skinInstances);

            if (!skinInstances.empty()) {
                uint32_t bb = app->getD3D12()->getCurrentBackBufferIdx();

                m_skinningPass->dispatch(cmd, skinInstances, bb);

                m_lastSkinInstances = skinInstances;
            }
        }
    }

    cmd->EndQuery(m_gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);

    ID3D12DescriptorHeap* heaps[] = { descs->getHeap(), app->getSamplerHeap()->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    handleNewScenePopup(cmd);

    if (m_sceneView->viewport.isReady()) m_sceneView->renderToTexture(cmd);
    if (m_gameView->viewport.isReady())  m_gameView->renderToTexture(cmd);

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
    if (m_memoryUpdateTimer >= 1000.0f) {
        m_memoryUpdateTimer = 0.0f;
        updateMemory();
    }
}

void ModuleEditor::renderSceneWithCamera(ID3D12GraphicsCommandList* cmd, const Matrix& view, const Matrix& proj, uint32_t w, uint32_t h, bool editorExtras) {
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

    std::vector<MeshEntry>  ownedEntries;
    std::vector<MeshEntry*> visibleMeshes;

    if (moduleScene) {
        std::function<void(GameObject*)> collectMeshes = [&](GameObject* node) {
            if (!node || !node->isActive()) return;
            
            if (auto* cm = node->getComponent<ComponentMesh>()) {
                cm->flushDeferredReleases();
                Matrix nodeWorld = node->getTransform()->getGlobalMatrix();
                if (Model* model = cm->getProceduralModel()) {
                    model->buildMeshEntries(nodeWorld, ownedEntries);

                    if (cm->isSkinned()) {
                        for (auto& e : ownedEntries) {
                            if (e.meshRes == nullptr) continue;

                            for (uint32_t si = 0; si < (uint32_t)m_lastSkinInstances.size(); ++si) {
                                if (m_lastSkinInstances[si].mesh == cm) {
                                    uint32_t bbIdx = app->getD3D12()->getCurrentBackBufferIdx();

                                    e.skinnedVertexVA = m_skinningPass->getSkinnedVA(si, bbIdx);
                                    e.skinnedVertexCount = m_lastSkinInstances[si].numVertices;

                                    static const float id[16] = {
                                        1,0,0,0,
                                        0,1,0,0,
                                        0,0,1,0,
                                        0,0,0,1
                                    };
                                    memcpy(e.worldMatrix, id, sizeof(id));
                                    break;
                                }
                            }
                        }
                    }
                }
                else {
                    for (const auto& src : cm->getEntries()) {
                        if (!src.meshRes || !src.materialRes) continue;

                        MeshEntry e;
                        e.meshUID = src.meshUID;
                        e.materialUID = src.materialUID;
                        e.meshRes = src.meshRes;
                        e.materialRes = src.materialRes;
                        e.materialCB = src.materialCB;

                        memcpy(e.worldMatrix, &nodeWorld, sizeof(nodeWorld));

                        if (cm->isSkinned()) {
                            for (uint32_t si = 0; si < (uint32_t)m_lastSkinInstances.size(); ++si) {
                                if (m_lastSkinInstances[si].mesh == cm) {
                                    uint32_t bbIdx = app->getD3D12()->getCurrentBackBufferIdx();

                                    e.skinnedVertexVA = m_skinningPass->getSkinnedVA(si, bbIdx);
                                    e.skinnedVertexCount = m_lastSkinInstances[si].numVertices;

                                    static const float id[16] = {
                                        1,0,0,0,
                                        0,1,0,0,
                                        0,0,1,0,
                                        0,0,0,1
                                    };
                                    memcpy(e.worldMatrix, id, sizeof(id));
                                    break;
                                }
                            }
                        }
                        ownedEntries.push_back(std::move(e));
                    }
                }
            }
            for (auto* child : node->getChildren())
                collectMeshes(child);
        };
        collectMeshes(moduleScene->getRoot());
        visibleMeshes.reserve(ownedEntries.size());
        for (auto& e : ownedEntries) visibleMeshes.push_back(&e);
    }

    const EnvironmentSystem* envForIBL =
        (sky.enabled && m_envSystem) ? m_envSystem.get() : nullptr;

    m_meshRenderPass->render(cmd, visibleMeshes, m_frameLights, camera->getPos(), view * proj, envForIBL, m_samplerType);

    if (editorExtras) {
        if (s.showGrid) dd::xzSquareGrid(-100.f, 100.f, 0.f, 1.f, dd::colors::Gray);
        if (s.showAxis) { Matrix id = Matrix::Identity; dd::axisTriad(id.m[0], 0.f, 2.f, 2.f); }
        if (s.debugDrawLights && moduleScene) debugDrawLights(moduleScene, s.debugLightSize);

        FrustumDebugDraw fdd;
        camera->buildDebugLines(fdd);
        for (const auto& line : fdd.lines) {
            ddVec3 f = { line.from.x, line.from.y, line.from.z };
            ddVec3 t = { line.to.x,   line.to.y,   line.to.z };
            const Vector3& c = line.color;
            if (c.x > .5f && c.y > .5f && c.z < .5f) dd::line(f, t, dd::colors::Yellow);
            else if (c.x < .5f && c.y > .5f && c.z < .5f) dd::line(f, t, dd::colors::Green);
            else if (c.x < .5f && c.y > .5f && c.z > .5f) dd::line(f, t, dd::colors::Cyan);
            else if (c.x > .5f && c.y < .5f && c.z < .5f) dd::line(f, t, dd::colors::Red);
            else if (c.x < .5f && c.y < .5f && c.z > .5f) dd::line(f, t, dd::colors::Blue);
            else                                             dd::line(f, t, dd::colors::White);
        }
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
        ImGuiID left, right, bottom, center, rightPanel;
        ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, 0.20f, &left, &right);
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.25f, &bottom, &right);
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Right, 0.28f, &rightPanel, &center);
        ImGui::DockBuilderDockWindow("Hierarchy", left);
        ImGui::DockBuilderDockWindow("Inspector", rightPanel);
        ImGui::DockBuilderDockWindow("Scene Settings", rightPanel);
        ImGui::DockBuilderDockWindow("Scene View", center);
        ImGui::DockBuilderDockWindow("Game View", center);
        ImGui::DockBuilderDockWindow("Console", bottom);
        ImGui::DockBuilderDockWindow("Asset Browser", bottom);
        ImGui::DockBuilderDockWindow("Resources", bottom);
        ImGui::DockBuilderDockWindow("Prefabs", bottom);
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

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene", "Ctrl+N"))       m_showNewSceneConfirm = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Save Scene", "Ctrl+S"))       saveScene();
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) m_saveDialog->open(FileDialog::Type::Save, "Save Scene", "Library/Scenes/");
        if (ImGui::MenuItem("Load Scene...", "Ctrl+O"))     m_loadDialog->open(FileDialog::Type::Open, "Load Scene", "Library/Scenes/");
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")) {}
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo()))                    undoToSavePoint();
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo()))                    redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, m_selection.has()))            copySelected();
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_clipboard.serialized.empty())) pasteClipboard();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, m_selection.has()))            duplicateSelected();
        ImGui::Separator();
        if (ImGui::MenuItem("Create Empty", "Ctrl+Shift+N")) createEmptyGameObject();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        for (EditorPanel* p : m_panels) ImGui::MenuItem(p->getName(), nullptr, &p->open);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("GameObject")) {
        if (ImGui::MenuItem("Create Empty")) createEmptyGameObject();
        if (ImGui::MenuItem("Create Empty Child") && m_selection.has()) createEmptyGameObject("Empty", m_selection.object);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Scene")) {
        if (ImGui::MenuItem("Play", nullptr, false, m_sceneManager && !m_sceneManager->isPlaying())) m_sceneManager->play();
        if (ImGui::MenuItem("Pause", nullptr, false, m_sceneManager && m_sceneManager->isPlaying()))  m_sceneManager->pause();
        if (ImGui::MenuItem("Stop", nullptr, false, m_sceneManager && m_sceneManager->getState() != SceneManager::PlayState::Stopped)) m_sceneManager->stop();
        ImGui::EndMenu();
    }

    {
        bool playing = m_sceneManager && m_sceneManager->isPlaying();
        bool paused = m_sceneManager && m_sceneManager->getState() == SceneManager::PlayState::Paused;
        const float btnW = 70.0f;
        const float total = btnW * 3 + ImGui::GetStyle().ItemSpacing.x * 2;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - total) * 0.5f);
        if (playing) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1));
        if (ImGui::Button("  Play  ", ImVec2(btnW, 0)) && m_sceneManager && !playing) m_sceneManager->play();
        if (playing) ImGui::PopStyleColor();
        ImGui::SameLine();
        if (paused) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.7f, 0.1f, 1));
        if (ImGui::Button(" Pause  ", ImVec2(btnW, 0)) && m_sceneManager && playing) m_sceneManager->pause();
        if (paused) ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("  Stop  ", ImVec2(btnW, 0)) && m_sceneManager) m_sceneManager->stop();
    }
    ImGui::EndMainMenuBar();
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
    if (ext == ".gltf" || ext == ".fbx" || ext == ".obj") {
        if (!scene) return;
        std::string stem = fs::path(path).stem().string();
        GameObject* go = scene->createGameObject(stem.c_str());
        bool ok = go->createComponent<ComponentMesh>()->loadModel(path.c_str());
        log(ok ? ("Added: " + stem).c_str() : ("Failed: " + path).c_str(), ok ? EditorColors::Success : EditorColors::Danger);
        if (ok) m_selection.object = go;
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

bool ModuleEditor::isChildOf(const GameObject* root, const GameObject* needle) {
    if (!root || !needle) return false;
    if (root == needle) return true;
    for (const auto* c : root->getChildren()) if (isChildOf(c, needle)) return true;
    return false;
}

void ModuleEditor::updateMemory() {
    uint64_t gpuMB = 0, ramMB = 0;
    if (ID3D12Device* device = app->getD3D12()->getDevice()) {
        ComPtr<IDXGIDevice>  dxgiDev;
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

void ModuleEditor::handleShortcuts() {
    if (ImGui::GetIO().WantTextInput) return;
    ImGuiIO& io = ImGui::GetIO();
    bool     ctrl = io.KeyCtrl;
    bool     shift = io.KeyShift;
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

void ModuleEditor::collectSkinInstances(GameObject* go, std::vector<SkinInstance>& out) {
    if (!go || !go->isActive()) return;
    if (auto* cm = go->getComponent<ComponentMesh>()) {
        if (cm->isSkinned()) {
            out.push_back(buildSkinInstance(cm));
        }
    }
    for (auto* c : go->getChildren())
        collectSkinInstances(c, out);
}

SkinInstance ModuleEditor::buildSkinInstance(ComponentMesh* cm) {
    SkinInstance si;
    si.mesh = cm;
    si.skin = cm->getSkinResource();
    si.numVertices = cm->getTotalVertexCount();
    si.morphWeights = cm->getMorphWeightsVec(); 

    if (!si.skin) return si;

    uint32_t jc = (uint32_t)si.skin->jointNames.size();
    si.paletteModel.resize(jc, Matrix::Identity);
    si.paletteNormal.resize(jc, Matrix::Identity);

    ModuleScene* scene = getActiveModuleScene();
    for (uint32_t j = 0; j < jc; ++j) {
        const std::string& jName = si.skin->jointNames[j];
        GameObject* jGO = scene ? scene->findGameObjectByName(jName) : nullptr;
        Matrix Tj = jGO
            ? jGO->getTransform()->getGlobalMatrix()
            : Matrix::Identity;
        Matrix Bj_inv = si.skin->inverseBindMatrices[j];
        si.paletteModel[j] = Bj_inv * Tj;

        Matrix inv;
        si.paletteModel[j].Invert(inv);
        si.paletteNormal[j] = inv; 
    }
    return si;
}

