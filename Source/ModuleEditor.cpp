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
#include "ForwardMeshPass.h"
#include "GBufferPass.h"
#include "DeferredLightingPass.h"
#include "DecalPass.h"
#include "ComponentDecal.h"
#include "ComponentBillboard.h"
#include "ComponentParticleSystem.h"
#include "ComponentTrail.h"
#include "RenderTexture.h"
#include "EmptyScene.h"
#include "SceneGraph.h"
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
#include "ModuleStaticBuffer.h"
#include <filesystem>
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <cfloat>

namespace fs = std::filesystem;


ModuleEditor::ModuleEditor() = default;
ModuleEditor::~ModuleEditor() = default;

ComPtr<ID3D12Resource> ModuleEditor::createUploadBuffer(ID3D12Device* device, SIZE_T size, const wchar_t* name){
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bd = CD3DX12_RESOURCE_DESC::Buffer((size + 255) & ~255);
    ComPtr<ID3D12Resource> buf;
    device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf));
    if (name) buf->SetName(name);
    return buf;
}

bool ModuleEditor::init(){
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
    m_collisionSystem = std::make_unique<CollisionSystem>();
    m_collisionResponse = std::make_unique<CollisionResponse>();
    m_sceneManager = std::make_unique<SceneManager>();
    m_meshRenderPass = std::make_unique<ForwardMeshPass>();
    m_hotReload = std::make_unique<HotReloadManager>();

    m_hotReload->setReloadCallback([this](const std::string& dllPath){
        notifyScriptComponentsReload(dllPath);
        });

    std::string scriptDir = app->getFileSystem()->GetAssetsPath() + std::string("Scripts/");
    app->getFileSystem()->CreateDir(scriptDir.c_str());

    m_scriptWatcher.start(scriptDir,
        [this](const std::string& absPath, FileWatcher::Event ev){
            onScriptFileEvent(absPath, ev);
        });

    auto existing = app->getFileSystem()->GetFilesInDirectory(scriptDir.c_str(), ".dll");
    for (const auto& path : existing)
        m_hotReload->loadLibrary(path);

    if (!m_meshRenderPass->init(device)) return false;

    m_skinningPass = std::make_unique<SkinningPass>();
    if (!m_skinningPass->init(device)){
        m_skinningPass.reset();
    }

    m_gbufferPass = std::make_unique<GBufferPass>();
    if (!m_gbufferPass->init(device)) return false;

    m_deferredLightingPass = std::make_unique<DeferredLightingPass>();
    if (!m_deferredLightingPass->init(device)) return false;

    m_decalPass = std::make_unique<DecalPass>();
    if (!m_decalPass->init(device)){
        LOG("ModuleEditor: DecalPass init failed (non-fatal)");
        m_decalPass.reset();
    }

    m_billboardPass = std::make_unique<BillboardPass>();
    if (!m_billboardPass->init(device)){
        LOG("ModuleEditor: BillboardPass init failed (non-fatal)");
        m_billboardPass.reset();
    }

    m_trailPass = std::make_unique<TrailPass>();
    if (!m_trailPass->init(device)){
        LOG("ModuleEditor: TrailPass init failed (non-fatal)");
        m_trailPass.reset();
    }

    m_particlePass = std::make_unique<ParticlePass>();
    if (!m_particlePass->init(device)){
        LOG("ModuleEditor: ParticlePass init failed (non-fatal)");
        m_particlePass.reset();
    }

    m_envSystem = std::make_unique<EnvironmentSystem>();
    if (!m_envSystem->init(device, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT, false)) return false;

    m_sceneManager->setScene(std::make_unique<EmptyScene>(), device);

    D3D12_QUERY_HEAP_DESC qd = { D3D12_QUERY_HEAP_TYPE_TIMESTAMP, 2, 0 };
    device->CreateQueryHeap(&qd, IID_PPV_ARGS(&m_gpuQueryHeap));
    auto rbH = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    auto rbD = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * 2);
    device->CreateCommittedResource(&rbH, D3D12_HEAP_FLAG_NONE, &rbD, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_gpuReadback));

    std::remove("imgui.ini");

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
    addPanel<GPUMemoryPanel>(this);

    HWND hwnd = app->getD3D12()->getHWnd();
    m_dropTarget = new EngineDropTarget();
    if (FAILED(RegisterDragDrop(hwnd, m_dropTarget))){
        m_dropTarget->Release();
        m_dropTarget = nullptr;
        log("[Editor] RegisterDragDrop failed — drag-drop overlay disabled", EditorColors::Warning);
    }

    DragDropManager::Get().SetRefreshCallback([this](){
        if (m_assetBrowser) m_assetBrowser->markDirty();
    });

    log("[Editor] Initialized", EditorColors::Success);
    return true;
}

bool ModuleEditor::cleanUp(){
    DragDropManager::Get().Shutdown();

    if (m_dropTarget){
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
    m_decalPass.reset();
    if (m_skinningPass){ m_skinningPass->cleanUp(); m_skinningPass.reset(); }
    m_gpuQueryHeap.Reset();
    m_gpuReadback.Reset();

    m_scriptWatcher.stop();
    m_hotReload->unloadAll();

    return true;
}

SceneGraph* ModuleEditor::getActiveModuleScene() const{
    return m_sceneManager ? m_sceneManager->getModuleScene() : nullptr;
}

void ModuleEditor::log(const char* text, const ImVec4& color){
    if (m_console) m_console->add(text, color);
}


bool ModuleEditor::isChildOf(const GameObject* root, const GameObject* needle){
    if (!root || !needle) return false;
    if (root == needle) return true;
    for (const auto* c : root->getChildren()) if (isChildOf(c, needle)) return true;
    return false;
}

void ModuleEditor::updateMemory(){
    uint64_t gpuMB = 0, ramMB = 0;
    if (ID3D12Device* device = app->getD3D12()->getDevice()){
        ComPtr<IDXGIDevice> dxgiDev;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIAdapter3> adapter3;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgiDev))) && dxgiDev)
            if (SUCCEEDED(dxgiDev->GetAdapter(&adapter)) && adapter)
                if (SUCCEEDED(adapter.As(&adapter3)) && adapter3){
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


ImVec2 ModuleEditor::getSceneViewSize() const{
    return m_sceneView ? m_sceneView->viewport.size : ImVec2(0, 0);
}


void ModuleEditor::stopPlay(){
    if (m_sceneManager) m_sceneManager->stop();
    m_selection.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_savePointIndex = 0;
}


void ModuleEditor::enterPrefabEdit(const std::string& prefabName){
    if (!m_sceneManager) return;
    app->getD3D12()->flush();
    if (m_sceneManager->isEditingPrefab()) m_sceneManager->exitPrefabEdit();
    m_prefabSession.clear();
    m_prefabSession.isolatedScene = std::make_unique<SceneGraph>();
    GameObject* loaded = PrefabManager::instantiatePrefab(prefabName, m_prefabSession.isolatedScene.get());
    if (!loaded){
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

void ModuleEditor::exitPrefabEdit(){
    if (!m_sceneManager || !m_sceneManager->isEditingPrefab()) return;
    m_pendingExitPrefab = true;
}

void ModuleEditor::flushExitPrefabEdit(){
    if (!m_pendingExitPrefab) return;
    m_pendingExitPrefab = false;
    app->getD3D12()->flush();
    m_selection.clear();
    m_sceneManager->exitPrefabEdit();
    m_prefabSession.clear();
    log("Exited prefab edit.", EditorColors::Muted);
}

void ModuleEditor::onScriptFileEvent(const std::string& absPath, FileWatcher::Event ev){
    if (absPath.size() < 4) return;
    std::string ext = absPath.substr(absPath.size() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".dll") return;

    switch (ev){
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

void ModuleEditor::notifyScriptComponentsReload(const std::string& ){
    auto* scene = getActiveModuleScene();
    if (!scene) return;

    std::function<void(GameObject*)> visit = [&](GameObject* node){
        if (!node) return;
        for (const auto& comp : node->getComponents()){
            if (comp->getType() == Component::Type::Script){
                auto* sc = static_cast<ComponentScript*>(comp.get());
                sc->onDllReloaded(m_hotReload.get());
            }
        }
        for (auto* child : node->getChildren())
            visit(child);
        };
    visit(scene->getRoot());
}
