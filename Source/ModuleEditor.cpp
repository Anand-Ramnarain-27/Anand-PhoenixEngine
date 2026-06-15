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
#include "DecalPass.h"
#include "ComponentDecal.h"
#include "ComponentBillboard.h"
#include "ComponentParticleSystem.h"
#include "ComponentTrail.h"
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
static constexpr float kDeg2Rad = 0.0174532925f;

static float computeScreenCoverage(const Vector3& mn, const Vector3& mx, const Matrix& viewProj){
    Vector3 corners[8] = {
        {mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mn.x,mx.y,mn.z},{mx.x,mx.y,mn.z},
        {mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mn.x,mx.y,mx.z},{mx.x,mx.y,mx.z},
    };
    Vector2 ndcMin(FLT_MAX, FLT_MAX), ndcMax(-FLT_MAX, -FLT_MAX);
    bool anyInFront = false;
    for (const auto& c : corners){
        Vector4 clip = Vector4::Transform(Vector4(c.x, c.y, c.z, 1.0f), viewProj);
        if (clip.w <= 0.0001f) continue;
        anyInFront = true;
        float x = clip.x / clip.w;
        float y = clip.y / clip.w;
        ndcMin.x = std::min(ndcMin.x, x); ndcMax.x = std::max(ndcMax.x, x);
        ndcMin.y = std::min(ndcMin.y, y); ndcMax.y = std::max(ndcMax.y, y);
    }
    if (!anyInFront) return 0.0f;
    ndcMin.x = std::max(ndcMin.x, -1.0f); ndcMax.x = std::min(ndcMax.x, 1.0f);
    ndcMin.y = std::max(ndcMin.y, -1.0f); ndcMax.y = std::min(ndcMax.y, 1.0f);
    float w = std::max(0.0f, ndcMax.x - ndcMin.x);
    float h = std::max(0.0f, ndcMax.y - ndcMin.y);
    return (w * h) / 4.0f;
}
static constexpr float kStatusH = 22.f;

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
    m_meshRenderPass = std::make_unique<MeshRenderPass>();
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

ModuleScene* ModuleEditor::getActiveModuleScene() const{
    return m_sceneManager ? m_sceneManager->getModuleScene() : nullptr;
}

void ModuleEditor::log(const char* text, const ImVec4& color){
    if (m_console) m_console->add(text, color);
}

void ModuleEditor::preRender(){
    flushExitPrefabEdit();
    m_sceneView->handleResize();
    m_gameView->handleResize();
    m_imguiPass->startFrame();
    ImGuizmo::BeginFrame();
    handleShortcuts();
    const float dt = static_cast<float>(app->getElapsedMilis()) * 0.001f;

    if (m_sceneManager){
        m_sceneManager->update(dt);
        m_sceneManager->updateAnimations(dt);
    }

    if (ModuleCamera* cam = app->getCamera()){
        ModuleScene* scene = getActiveModuleScene();
        int visible = 0, total = 0;
        if (scene){
            std::vector<RenderOctree::Entry> entries;
            std::function<void(GameObject*)> collect = [&](GameObject* node){
                if (!node || !node->isActive()) return;
                if (auto* cm = node->getComponent<ComponentMesh>()){
                    if (cm->hasAABB()){
                        Vector3 mn, mx;
                        cm->getWorldAABB(mn, mx);
                        entries.push_back({ node, AABB{ mn, mx } });
                        ++total;
                    } else {
                        cm->setVisible(true);
                    }
                }
                for (auto* child : node->getChildren()) collect(child);
            };
            collect(scene->getRoot());

            if (cam->cullAlgorithm == ModuleCamera::CullAlgorithm::Octree){
                m_renderOctree.clear();
                for (const auto& e : entries) m_renderOctree.add(e.go, e.worldAABB);
                m_renderOctree.build();
                cam->octreeNodeCount = m_renderOctree.getNodeCount();
                cam->octreeLeafCount = m_renderOctree.getLeafCount();

                if (!cam->hasGameFrustum()){
                    for (const auto& e : entries){ e.go->getComponent<ComponentMesh>()->setVisible(true); ++visible; }
                } else {
                    std::vector<GameObject*> visibleSet;
                    m_renderOctree.query(cam->getGameFrustum(), visibleSet);
                    std::unordered_set<GameObject*> visibleLookup(visibleSet.begin(), visibleSet.end());
                    for (const auto& e : entries){
                        bool vis = visibleLookup.count(e.go) != 0;
                        e.go->getComponent<ComponentMesh>()->setVisible(vis);
                        if (vis) ++visible;
                    }
                }
            } else {
                cam->octreeNodeCount = 0;
                cam->octreeLeafCount = 0;
                for (const auto& e : entries){
                    bool vis = !cam->hasGameFrustum() || cam->getGameFrustum().intersectsAABB(e.worldAABB.min, e.worldAABB.max);
                    e.go->getComponent<ComponentMesh>()->setVisible(vis);
                    if (vis) ++visible;
                }
            }
        }
        cam->setVisibilityStats(visible, total);
    }

    if (m_effectsPlaying && m_sceneManager &&
        m_sceneManager->getState() != SceneManager::PlayState::Playing){
        updateEffectsInEditMode(dt);
    }

    ModuleScene* activeScene = getActiveModuleScene();
    if (m_collisionSystem)
        m_collisionSystem->run(activeScene, dt);

    const bool isPlaying = m_sceneManager &&
        m_sceneManager->getState() == SceneManager::PlayState::Playing;
    if (isPlaying && m_collisionResponse && m_collisionSystem)
        m_collisionResponse->solve(m_collisionSystem->getResults().contacts, dt);

    m_performance->pushFPS(app->getFPS());

    DragDropManager::Get().Update();

    drawDockspace();
    drawMenuBar();
    for (EditorPanel* p : m_panels) if (p->open) p->draw();



    handleDialogs();
    drawStatusBar();

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

void ModuleEditor::renderSceneWithCamera(ID3D12GraphicsCommandList* cmd, const Matrix& view, const Matrix& proj, uint32_t w, uint32_t h, bool editorExtras, RenderTexture* outputRT){
    ModuleCamera* camera = app->getCamera();
    ModuleScene* moduleScene = getActiveModuleScene();

    Matrix viewCamWorld; view.Invert(viewCamWorld);
    const Vector3 viewCamPos = viewCamWorld.Translation();
    Vector3 viewCamRight = Vector3::TransformNormal(Vector3::UnitX, viewCamWorld); viewCamRight.Normalize();
    Vector3 viewCamUp = Vector3::TransformNormal(Vector3::UnitY, viewCamWorld); viewCamUp.Normalize();

    if (moduleScene){
        std::function<void(GameObject*)> flush = [&](GameObject* node){
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

    std::vector<SkinningPass::SkinJob> skinJobs;
    std::vector<size_t> skinJobEntryIdx;
    uint32_t curPaletteOffset = 0;
    uint32_t curVertexOffset = 0;
    uint32_t curMorphWeightOffset = 0;

    const Matrix lodViewProj = view * proj;
    const int forceLODIndex = (int)camera->forceLOD - 1;

    if (moduleScene){
        std::function<void(GameObject*)> collectMeshes = [&](GameObject* node){
            if (!node || !node->isActive()) return;
            if (auto* cm = node->getComponent<ComponentMesh>()){
                cm->flushDeferredReleases();

                if (!editorExtras && camera->cullMode == ModuleCamera::CullMode::Frustum && !cm->isVisible()){
                    for (auto* child : node->getChildren()) collectMeshes(child);
                    return;
                }

                if (cm->hasLODLevels() && cm->hasAABB()){
                    Vector3 mn, mx;
                    cm->getWorldAABB(mn, mx);
                    float coverage = computeScreenCoverage(mn, mx, lodViewProj);
                    cm->updateLOD(coverage, forceLODIndex);
                }

                Matrix nodeWorld = node->getTransform()->getGlobalMatrix();
                if (Model* model = cm->getProceduralModel()){
                    model->buildMeshEntries(nodeWorld, ownedEntries);
                }
                else {
                    const bool isSkinned = m_skinningPass && cm->hasSkinData();

                    const bool morphDirtyThisFrame = m_skinningPass && cm->getMorphWeightsDirty();
                    if (morphDirtyThisFrame) cm->clearMorphWeightsDirty();

                    for (const auto& src : cm->getEntries()){
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

                        bool shouldMorph = false;
                        if (m_skinningPass && mesh && mesh->hasMorphTargets()){
                            shouldMorph = morphDirtyThisFrame;
                            if (!shouldMorph){
                                const float* w = cm->getMorphWeights();
                                const uint32_t n = mesh->getNumMorphTargets();
                                for (uint32_t t = 0; t < n && !shouldMorph; ++t)
                                    shouldMorph = (w[t] != 0.f);
                            }
                        }

                        const bool vertexReady = mesh && (mesh->getVertexBufferVA() != 0);
                        const uint32_t vcount = mesh ? mesh->getVertexCount() : 0u;
                        const uint32_t jcount = hasBones ? (uint32_t)cm->getLocalSkin().jointNodeIndices.size() : 0u;
                        const bool withinVertexCap = (curVertexOffset + vcount <= SkinningPass::MAX_TOTAL_VERTICES);
                        const bool withinJointCap = (curPaletteOffset + jcount <= SkinningPass::MAX_TOTAL_JOINTS);
                        if (!withinVertexCap)
                            LOG("[SkinDebug] OVERFLOW: vertex cap %u exceeded (offset %u + count %u). Re-export at lower poly count.",
                                SkinningPass::MAX_TOTAL_VERTICES, curVertexOffset, vcount);
                        if (!withinJointCap)
                            LOG("[SkinDebug] OVERFLOW: joint cap %u exceeded (offset %u + count %u).",
                                SkinningPass::MAX_TOTAL_JOINTS, curPaletteOffset, jcount);
                        const bool needsGpuJob = vertexReady && (hasBones || shouldMorph) && withinVertexCap && withinJointCap;

                        if (needsGpuJob){
                            e.isSkinned = true;

                            SkinningPass::SkinJob job;
                            job.mesh = mesh;
                            job.paletteOffset = curPaletteOffset;
                            job.vertexOffset = curVertexOffset;
                            job.morphWeightOffset = curMorphWeightOffset;

                            if (hasBones){
                                const auto& joints = cm->getSkinJoints();
                                std::vector<Matrix> jointWorlds;
                                jointWorlds.reserve(joints.size());

                                int nullJointCount = 0;
                                for (auto* jgo : joints){
                                    if (!jgo) ++nullJointCount;
                                    jointWorlds.push_back(jgo ? jgo->getTransform()->getGlobalMatrix() : Matrix::Identity);
                                }
                                if (nullJointCount > 0)
                                    LOG("[SkinDebug] WARNING: %d/%d joint GOs are null",
                                        nullJointCount, (int)joints.size());

                                job.skin = &cm->getLocalSkin();
                                job.jointWorldMatrices = std::move(jointWorlds);

                                Matrix inv; nodeWorld.Invert(inv);
                                job.meshWorldInverse = inv;
                                memcpy(e.worldMatrix, &nodeWorld, sizeof(nodeWorld));
                            } else {
                                memcpy(e.worldMatrix, &nodeWorld, sizeof(nodeWorld));
                            }

                            if (shouldMorph){
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

    m_frameDrawCalls = (int)visibleMeshes.size();
    m_frameMeshCount = m_frameDrawCalls;

    if (!skinJobs.empty() && m_skinningPass){
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

    std::vector<MeshEntry*> opaqueMeshes;
    std::vector<MeshEntry*> translucentMeshes;
    opaqueMeshes.reserve(visibleMeshes.size());
    translucentMeshes.reserve(visibleMeshes.size());
    for (MeshEntry* e : visibleMeshes){
        const Material* mat = e->instanceMaterial.get();
        if (!mat) mat = e->material;
        if (!mat && e->materialRes) mat = e->materialRes->getMaterial();
        bool isTranslucent = mat && mat->getData().baseColor.w < 0.999f;
        (isTranslucent ? translucentMeshes : opaqueMeshes).push_back(e);
    }

    std::vector<BillboardInstance> billboards;
    if (m_billboardPass && moduleScene){
        gatherBillboards(moduleScene->getRoot(), billboards, view, viewProj,
                         viewCamPos, viewCamRight, viewCamUp);
        gatherParticleSystems(moduleScene->getRoot(), billboards, viewProj,
                              viewCamPos, viewCamRight, viewCamUp);
    }

    std::vector<TrailInstance> trails;
    if (m_trailPass && moduleScene){
        gatherTrails(moduleScene->getRoot(), trails, viewProj, viewCamPos);
    }

    std::vector<ParticleDrawRequest> gpuParticleRequests;
    if (m_particlePass && moduleScene){
        gatherGPUParticles(moduleScene->getRoot(), gpuParticleRequests,
                           viewCamPos, viewCamRight, viewCamUp,
                           (float)app->getElapsedMilis() / 1000.f);
    }

    if (m_gbufferPass && (!opaqueMeshes.empty() || !translucentMeshes.empty() || !billboards.empty())){
        const int gbufferViewportIndex = editorExtras ? 0 : 1;
        m_gbufferPass->render(cmd, opaqueMeshes, viewProj, w, h, gbufferViewportIndex);

        if (outputRT && outputRT->isValid()){
            auto rtv = outputRT->getRtvHandle();
            auto dsv = outputRT->getDsvHandle();
            bool hasDsv = outputRT->getDepthTexture() != nullptr;
            cmd->OMSetRenderTargets(1, &rtv, FALSE, hasDsv ? &dsv : nullptr);
            D3D12_VIEWPORT vp = { 0.f, 0.f, float(w), float(h), 0.f, 1.f };
            D3D12_RECT sc = { 0, 0, LONG(w), LONG(h) };
            cmd->RSSetViewports(1, &vp);
            cmd->RSSetScissorRects(1, &sc);
        }

        if (m_decalPass && moduleScene){
            std::vector<DecalInstance> decals;
            gatherDecals(moduleScene->getRoot(), decals, view, proj, w, h);
            if (!decals.empty())
                m_decalPass->render(cmd, *m_gbufferPass, decals, w, h);
        }

        if (m_deferredLightingPass){
            Matrix invViewProj;
            viewProj.Invert(invViewProj);

            if (!editorExtras && camera->hasGameFrustum()){
                const Frustum& gf = camera->getGameFrustum();
                FrameLightData culledLights;
                culledLights.dirLights = m_frameLights.dirLights;
                culledLights.pointLights.reserve(m_frameLights.pointLights.size());
                for (const auto& pl : m_frameLights.pointLights){
                    Sphere s{ pl.position, sqrtf(pl.squaredRadius) };
                    AABB box = s.toAABB();
                    if (gf.intersectsAABB(box.min, box.max)) culledLights.pointLights.push_back(pl);
                }
                culledLights.spotLights.reserve(m_frameLights.spotLights.size());
                for (const auto& sl : m_frameLights.spotLights){
                    Sphere s{ sl.position, sqrtf(sl.squaredRadius) };
                    AABB box = s.toAABB();
                    if (gf.intersectsAABB(box.min, box.max)) culledLights.spotLights.push_back(sl);
                }
                m_deferredLightingPass->render(cmd, *m_gbufferPass, culledLights,
                                                viewCamPos, view, proj,
                                                invViewProj, envForIBL, w, h,
                                                gbufferViewportIndex);
            } else {
                m_deferredLightingPass->render(cmd, *m_gbufferPass, m_frameLights,
                                                viewCamPos, view, proj,
                                                invViewProj, envForIBL, w, h,
                                                gbufferViewportIndex);
            }
        }

        if (!translucentMeshes.empty() && m_meshRenderPass && outputRT && outputRT->isValid()){
            const Vector3 camPos = viewCamPos;
            std::sort(translucentMeshes.begin(), translucentMeshes.end(),
                      [&camPos](const MeshEntry* a, const MeshEntry* b){
                          Matrix wa, wb;
                          memcpy(&wa, a->worldMatrix, sizeof(float) * 16);
                          memcpy(&wb, b->worldMatrix, sizeof(float) * 16);
                          float da = Vector3::DistanceSquared(wa.Translation(), camPos);
                          float db = Vector3::DistanceSquared(wb.Translation(), camPos);
                          return da > db;
                      });

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

        if (m_billboardPass && moduleScene && outputRT && outputRT->isValid()){
            const Vector3 camPos = viewCamPos;
            if (!billboards.empty()){
                std::sort(billboards.begin(), billboards.end(),
                          [&camPos](const BillboardInstance& a, const BillboardInstance& b){
                              if (a.additive != b.additive) return !a.additive && b.additive;
                              Vector3 pa(a.cb.centerHalfWidth.x, a.cb.centerHalfWidth.y, a.cb.centerHalfWidth.z);
                              Vector3 pb(b.cb.centerHalfWidth.x, b.cb.centerHalfWidth.y, b.cb.centerHalfWidth.z);
                              float da = Vector3::DistanceSquared(pa, camPos);
                              float db = Vector3::DistanceSquared(pb, camPos);
                              return da > db;
                          });

                auto rtv = outputRT->getRtvHandle();
                auto roDsv = m_gbufferPass->getGBuffer().getReadOnlyDsvHandle();
                cmd->OMSetRenderTargets(1, &rtv, FALSE, &roDsv);
                D3D12_VIEWPORT vp = { 0.f, 0.f, float(w), float(h), 0.f, 1.f };
                D3D12_RECT sc = { 0, 0, LONG(w), LONG(h) };
                cmd->RSSetViewports(1, &vp);
                cmd->RSSetScissorRects(1, &sc);

                m_billboardPass->render(cmd, billboards, w, h);
            }
        }

        if (m_trailPass && moduleScene && outputRT && outputRT->isValid() && !trails.empty()){
            const Vector3 camPos = viewCamPos;
            std::sort(trails.begin(), trails.end(),
                      [&camPos](const TrailInstance& a, const TrailInstance& b){
                          if (a.additive != b.additive) return !a.additive && b.additive;
                          float da = Vector3::DistanceSquared(a.sortPos, camPos);
                          float db = Vector3::DistanceSquared(b.sortPos, camPos);
                          return da > db;
                      });

            auto rtv = outputRT->getRtvHandle();
            auto roDsv = m_gbufferPass->getGBuffer().getReadOnlyDsvHandle();
            cmd->OMSetRenderTargets(1, &rtv, FALSE, &roDsv);
            D3D12_VIEWPORT vp = { 0.f, 0.f, float(w), float(h), 0.f, 1.f };
            D3D12_RECT sc = { 0, 0, LONG(w), LONG(h) };
            cmd->RSSetViewports(1, &vp);
            cmd->RSSetScissorRects(1, &sc);

            m_trailPass->render(cmd, trails, viewProj, w, h);
        }

        if (m_particlePass && moduleScene && outputRT && outputRT->isValid()
                           && !gpuParticleRequests.empty()){
            const Vector3 camPos = viewCamPos;
            std::sort(gpuParticleRequests.begin(), gpuParticleRequests.end(),
                      [&camPos](const ParticleDrawRequest& a, const ParticleDrawRequest& b){
                          if (a.additive != b.additive) return !a.additive && b.additive;
                          if (a.particles.empty() || b.particles.empty()) return false;
                          const auto& pa = a.particles.front();
                          const auto& pb = b.particles.front();
                          Vector3 pa3(pa.position[0], pa.position[1], pa.position[2]);
                          Vector3 pb3(pb.position[0], pb.position[1], pb.position[2]);
                          return Vector3::DistanceSquared(pa3, camPos)
                               > Vector3::DistanceSquared(pb3, camPos);
                      });

            auto rtv = outputRT->getRtvHandle();
            auto roDsv = m_gbufferPass->getGBuffer().getReadOnlyDsvHandle();
            cmd->OMSetRenderTargets(1, &rtv, FALSE, &roDsv);
            D3D12_VIEWPORT vp = { 0.f, 0.f, float(w), float(h), 0.f, 1.f };
            D3D12_RECT sc = { 0, 0, LONG(w), LONG(h) };
            cmd->RSSetViewports(1, &vp);
            cmd->RSSetScissorRects(1, &sc);

            m_particlePass->render(cmd, gpuParticleRequests, viewProj,
                                   camera->getRight(), camera->getUp(),
                                   (float)app->getElapsedMilis() / 1000.f,
                                   w, h);
        }

        ID3D12DescriptorHeap* heaps2[] = { app->getShaderDescriptors()->getHeap(),
                                           app->getSamplerHeap()->getHeap() };
        cmd->SetDescriptorHeaps(2, heaps2);
    }

    if (editorExtras){
        if (s.showGrid) dd::xzSquareGrid(-100.f, 100.f, 0.f, 1.f, dd::colors::Gray);
        if (s.showAxis){ Matrix id = Matrix::Identity; dd::axisTriad(id.m[0], 0.f, 2.f, 2.f); }
        if (s.debugDrawLights && moduleScene) debugDrawLights(moduleScene, s.debugLightSize);

        FrustumDebugDraw fdd;
        camera->buildDebugLines(fdd);
        for (const auto& line : fdd.lines){
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
        if (moduleScene){
            std::function<void(GameObject*)> drawGizmos = [&](GameObject* node){
                if (!node || !node->isActive()) return;
                for (const auto& comp : node->getComponents())
                    comp->onDrawGizmos();
                for (auto* child : node->getChildren())
                    drawGizmos(child);
            };
            drawGizmos(moduleScene->getRoot());
        }

        if (s.debugDrawBounds && moduleScene){
            struct BoundsEntry {
                BVType type;
                AABB box;
                Sphere sphere;
            };
            std::vector<BoundsEntry> boundsEntries;

            std::function<void(GameObject*)> collectBounds = [&](GameObject* node){
                if (!node || !node->isActive()) return;
                if (auto* cm = node->getComponent<ComponentMesh>()){
                    if (cm->hasAABB()){
                        BoundsEntry e;
                        const ComponentBounds* cb = node->getComponent<ComponentBounds>();

                        if (cb && cb->bvType == BVType::Sphere){
                            const Matrix& W = node->getTransform()->getGlobalMatrix();
                            const Vector3 lMin = cm->getLocalAABBMin();
                            const Vector3 lMax = cm->getLocalAABBMax();
                            const Vector3 lHalf = (lMax - lMin) * 0.5f;
                            const Vector3 lCtr = (lMin + lMax) * 0.5f;
                            Vector3 center = Vector3::Transform(lCtr, W);

                            Vector3 cx(W._11,W._12,W._13), cy(W._21,W._22,W._23), cz(W._31,W._32,W._33);
                            float hx = lHalf.x * cx.Length();
                            float hy = lHalf.y * cy.Length();
                            float hz = lHalf.z * cz.Length();
                            float radius = (cb->radiusOverride >= 0.f)
                                ? cb->radiusOverride
                                : sqrtf(hx*hx + hy*hy + hz*hz);

                            e.type = BVType::Sphere;
                            e.sphere = { center, radius };
                        } else {
                            Vector3 mn, mx;
                            cm->getWorldAABB(mn, mx);
                            e.type = BVType::AABB;
                            e.box = { mn, mx };
                        }
                        boundsEntries.push_back(e);
                    }
                }
                for (auto* child : node->getChildren()) collectBounds(child);
            };
            collectBounds(moduleScene->getRoot());

            const size_t N = boundsEntries.size();
            std::vector<bool> colliding(N, false);
            for (size_t i = 0; i < N; ++i){
                for (size_t j = i + 1; j < N; ++j){
                    bool hit = false;
                    const BoundsEntry& ei = boundsEntries[i];
                    const BoundsEntry& ej = boundsEntries[j];
                    if (ei.type == BVType::AABB && ej.type == BVType::AABB)
                        hit = ei.box.intersects(ej.box);
                    else if (ei.type == BVType::Sphere && ej.type == BVType::Sphere)
                        hit = ei.sphere.intersects(ej.sphere);
                    else if (ei.type == BVType::Sphere)
                        hit = ei.sphere.intersects(ej.box);
                    else
                        hit = ej.sphere.intersects(ei.box);
                    if (hit){ colliding[i] = true; colliding[j] = true; }
                }
            }

            for (size_t i = 0; i < N; ++i){
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

        if (camera->showFrustumCullingDebug){
            if (camera->hasGameFrustum()){
                FrustumDebugDraw gameFdd;
                gameFdd.addFrustum(camera->getGameFrustum(), Vector3(1.f, 0.5f, 0.f));
                for (const auto& line : gameFdd.lines){
                    ddVec3 f = { line.from.x, line.from.y, line.from.z };
                    ddVec3 t = { line.to.x, line.to.y, line.to.z };
                    dd::line(f, t, dd::colors::Orange);
                }
            }

            if (moduleScene){
                std::function<void(GameObject*)> drawCullDebug = [&](GameObject* node){
                    if (!node || !node->isActive()) return;
                    if (auto* cm = node->getComponent<ComponentMesh>(); cm && cm->hasAABB()){
                        Vector3 mn, mx;
                        cm->getWorldAABB(mn, mx);
                        const float* color = cm->isVisible() ? dd::colors::Green : dd::colors::Red;
                        dd::aabb(ddConvert(mn), ddConvert(mx), color);
                    }
                    for (auto* child : node->getChildren()) drawCullDebug(child);
                };
                drawCullDebug(moduleScene->getRoot());
            }
        }

        m_debugDraw->record(cmd, w, h, view, proj);
    }
}

void ModuleEditor::gatherLights(GameObject* node, FrameLightData& out) const{
    if (!node || !node->isActive()) return;

    if (auto* dl = node->getComponent<ComponentDirectionalLight>(); dl && dl->enabled){
        if (out.dirLights.size() < MeshPipeline::MAX_DIR_LIGHTS){
            MeshPipeline::GPUDirectionalLight g;
            g.direction = dl->direction;
            g.direction.Normalize();
            g.color = dl->color;
            g.intensity = dl->intensity;
            g._pad = 0.f;
            out.dirLights.push_back(g);
        }
    }

    if (auto* pl = node->getComponent<ComponentPointLight>(); pl && pl->enabled){
        if (out.pointLights.size() < MeshPipeline::MAX_POINT_LIGHTS){
            MeshPipeline::GPUPointLight p;
            p.position = node->getTransform()->getGlobalMatrix().Translation();
            p.squaredRadius = pl->radius * pl->radius;
            p.color = pl->color;
            p.intensity = pl->intensity;
            out.pointLights.push_back(p);
        }
    }

    if (auto* sl = node->getComponent<ComponentSpotLight>(); sl && sl->enabled){
        if (out.spotLights.size() < MeshPipeline::MAX_SPOT_LIGHTS){
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

void ModuleEditor::gatherDecals(GameObject* node, std::vector<DecalInstance>& out,
                                  const Matrix& view, const Matrix& proj,
                                  uint32_t w, uint32_t h) const{
    if (!node || !node->isActive()) return;

    if (auto* dc = node->getComponent<ComponentDecal>(); dc && dc->enabled){
        if (out.size() < DecalPass::MAX_DECALS){
            Matrix worldMat = node->getTransform()->getGlobalMatrix();
            Matrix viewProj = view * proj;

            DecalInstance inst;
            inst.mvp = (worldMat * viewProj).Transpose();
            worldMat.Invert(inst.invModel);
            inst.invModel = inst.invModel.Transpose();

            Matrix invVP;
            viewProj.Invert(invVP);
            inst.invViewProj = invVP.Transpose();

            inst.colourOpacity = Vector4(dc->colour.x, dc->colour.y, dc->colour.z, dc->opacity);

            out.push_back(inst);
        }
    }

    for (auto* c : node->getChildren()) gatherDecals(c, out, view, proj, w, h);
}

void ModuleEditor::gatherBillboards(GameObject* node, std::vector<BillboardInstance>& out,
                                     const Matrix& view, const Matrix& viewProj,
                                     const Vector3& camPos, const Vector3& camRight, const Vector3& camUp) const{
    if (!node || !node->isActive()) return;

    if (auto* bb = node->getComponent<ComponentBillboard>(); bb && bb->enabled){
        if (out.size() < BillboardPass::MAX_BILLBOARDS){
            const Vector3 center = node->getTransform()->getGlobalMatrix().Translation();

            Vector3 right, up;
            switch (bb->alignment){
            case ComponentBillboard::Alignment::Screen:
                right = camRight;
                up = camUp;
                break;
            case ComponentBillboard::Alignment::World: {
                Vector3 worldUp(0.f, 1.f, 0.f);
                Vector3 n = camPos - center;
                if (n.LengthSquared() < 1e-8f) n = -camRight;
                n.Normalize();
                right = worldUp.Cross(n);
                if (right.LengthSquared() < 1e-8f) right = camRight;
                right.Normalize();
                up = n.Cross(right);
                break;
            }
            case ComponentBillboard::Alignment::Axial:
            default: {
                Vector3 fixedUp(0.f, 1.f, 0.f);
                Vector3 toCam = camPos - center;
                right = toCam.Cross(fixedUp);
                if (right.LengthSquared() < 1e-8f) right = camRight;
                right.Normalize();
                up = fixedUp;
                break;
            }
            }

            const int cols = std::max(1, bb->sheetColumns);
            const int rows = std::max(1, bb->sheetRows);
            const int totalTiles = cols * rows;
            const float frame = bb->getCurrentFrame();
            const int frameA = ((int)frame) % totalTiles;
            const int frameB = (frameA + 1) % totalTiles;
            const float blend = frame - floorf(frame);

            auto tileRect = [cols, rows](int tileIndex) -> Vector4 {
                int tx = tileIndex % cols;
                int ty = tileIndex / cols;
                ty = (rows - 1) - ty;
                float u0 = (float)tx / (float)cols;
                float v0 = (float)ty / (float)rows;
                return Vector4(u0, v0, u0 + 1.f / cols, v0 + 1.f / rows);
            };

            BillboardInstance inst;
            inst.cb.viewProj = viewProj.Transpose();
            inst.cb.centerHalfWidth = Vector4(center.x, center.y, center.z, bb->size.x * 0.5f);
            inst.cb.rightHalfHeight = Vector4(right.x, right.y, right.z, bb->size.y * 0.5f);
            inst.cb.up = Vector4(up.x, up.y, up.z, 0.f);
            inst.cb.tint = bb->tint;
            inst.cb.frameRectA = tileRect(frameA);
            inst.cb.frameRectB = (totalTiles > 1) ? tileRect(frameB) : inst.cb.frameRectA;
            inst.cb.blendFactor = Vector4(blend, 0.f, 0.f, 0.f);
            inst.texturePath = bb->texturePath;

            out.push_back(std::move(inst));
        }
    }

    for (auto* c : node->getChildren()) gatherBillboards(c, out, view, viewProj, camPos, camRight, camUp);
}

void ModuleEditor::gatherParticleSystems(GameObject* node, std::vector<BillboardInstance>& out,
                                          const Matrix& viewProj,
                                          const Vector3& camPos, const Vector3& camRight, const Vector3& camUp) const{
    if (!node || !node->isActive()) return;

    if (auto* ps = node->getComponent<ComponentParticleSystem>();
        ps && ps->enabled && !ps->useGPU){
        const int cols = std::max(1, ps->sheetColumns);
        const int rows = std::max(1, ps->sheetRows);
        const int totalTiles = cols * rows;

        auto tileRect = [cols, rows](int tileIndex) -> Vector4 {
            int tx = tileIndex % cols;
            int ty = tileIndex / cols;
            ty = (rows - 1) - ty;
            float u0 = (float)tx / (float)cols;
            float v0 = (float)ty / (float)rows;
            return Vector4(u0, v0, u0 + 1.f / cols, v0 + 1.f / rows);
        };

        for (const auto& p : ps->getParticles()){
            if (!p.alive) continue;
            if (out.size() >= BillboardPass::MAX_BILLBOARDS) break;

            const float t = std::clamp(p.age / std::max(0.0001f, p.lifetime), 0.f, 1.f);
            const float size = p.baseSize * ps->sizeMultiplierAt(t);
            const Vector4 color = ps->colorAt(t);

            const float rad = p.rotationDeg * (3.14159265358979323846f / 180.f);
            const float cs = std::cos(rad), sn = std::sin(rad);
            const Vector3 right = camRight * cs + camUp * sn;
            const Vector3 up = camUp * cs - camRight * sn;

            const Vector4 frameA = tileRect(p.frameIndex % totalTiles);

            BillboardInstance inst;
            inst.cb.viewProj = viewProj.Transpose();
            inst.cb.centerHalfWidth = Vector4(p.position.x, p.position.y, p.position.z, size * 0.5f);
            inst.cb.rightHalfHeight = Vector4(right.x, right.y, right.z, size * 0.5f);
            inst.cb.up = Vector4(up.x, up.y, up.z, 0.f);
            inst.cb.tint = color;
            inst.cb.frameRectA = frameA;
            inst.cb.frameRectB = frameA;
            inst.cb.blendFactor = Vector4(0.f, 0.f, 0.f, 0.f);
            inst.texturePath = ps->texturePath;
            inst.additive = (ps->blendMode == ComponentParticleSystem::BlendMode::Additive);

            out.push_back(std::move(inst));
        }
    }

    for (auto* c : node->getChildren()) gatherParticleSystems(c, out, viewProj, camPos, camRight, camUp);
}

void ModuleEditor::gatherTrails(GameObject* node, std::vector<TrailInstance>& out,
                                const Matrix& viewProj, const Vector3& camPos) const{
    if (!node || !node->isActive()) return;

    if (auto* tr = node->getComponent<ComponentTrail>(); tr && tr->enabled){
        if (out.size() < TrailPass::MAX_TRAILS){
            TrailInstance inst;
            bool built = tr->buildMesh(camPos, inst.vertices);
            if (built && !inst.vertices.empty()){
                inst.tint = Vector4(1.f, 1.f, 1.f, 1.f);
                inst.texturePath = tr->texturePath;
                inst.additive = (tr->blendMode == ComponentTrail::BlendMode::Additive);
                inst.sortPos = inst.vertices.front().position;
                inst.layer = tr->layer;
                out.push_back(std::move(inst));
            }
        }
    }

    for (auto* c : node->getChildren()) gatherTrails(c, out, viewProj, camPos);
}

void ModuleEditor::gatherGPUParticles(GameObject* node,
                                       std::vector<ParticleDrawRequest>& out,
                                       const Vector3& ,
                                       const Vector3& , const Vector3& ,
                                       float elapsedTime) const {
    if (!node || !node->isActive()) return;

    if (auto* ps = node->getComponent<ComponentParticleSystem>();
        ps && ps->enabled && ps->useGPU){
        const int cols = std::max(1, ps->sheetColumns);
        const int rows = std::max(1, ps->sheetRows);
        const int totalTiles = cols * rows;

        auto tileUV = [cols, rows](int tileIdx) -> std::pair<Vector2, Vector2> {
            int tx = tileIdx % cols;
            int ty = tileIdx / cols;
            ty = (rows - 1) - ty;
            float u0 = (float)tx / cols, u1 = u0 + 1.f / cols;
            float v0 = (float)ty / rows, v1 = v0 + 1.f / rows;
            return { Vector2(u0, v0), Vector2(u1, v1) };
        };

        ParticleDrawRequest req;
        req.emitterKey = reinterpret_cast<size_t>(ps);
        req.maxParticles = ps->maxParticles;
        req.texturePath = ps->texturePath;
        req.additive = (ps->blendMode == ComponentParticleSystem::BlendMode::Additive);
        req.gpuTurbulence = ps->useTurbulence;
        req.turbFrequency = ps->turbulenceFrequency;
        req.turbStrength = ps->turbulenceStrength;
        req.turbScrollSpeed = ps->turbulenceScroll;
        req.time = elapsedTime;
        req.deltaTime = std::clamp((float)app->getElapsedMilis() * 0.001f, 0.f, 0.1f);

        for (const auto& p : ps->getParticles()){
            if (!p.alive) continue;
            if ((int)req.particles.size() >= ps->maxParticles) break;

            const float t = std::clamp(p.age / std::max(0.0001f, p.lifetime), 0.f, 1.f);
            const float size = p.baseSize * ps->sizeMultiplierAt(t);
            const Vector4 col = ps->colorAt(t);

            auto [uvMin, uvMax] = tileUV(p.frameIndex % totalTiles);

            GpuParticle gp;
            gp.position[0] = p.position.x;
            gp.position[1] = p.position.y;
            gp.position[2] = p.position.z;
            gp.size = size;
            gp.color[0] = col.x;
            gp.color[1] = col.y;
            gp.color[2] = col.z;
            gp.color[3] = col.w;
            gp.rotation = p.rotationDeg;
            gp.uvMin[0] = uvMin.x;
            gp.uvMin[1] = uvMin.y;
            gp.uvMax[0] = uvMax.x;
            gp.uvMax[1] = uvMax.y;
            req.particles.push_back(gp);
        }

        if (!req.particles.empty())
            out.push_back(std::move(req));
    }

    for (auto* c : node->getChildren())
        gatherGPUParticles(c, out, {}, {}, {}, elapsedTime);
}

void ModuleEditor::drawDockspace(){
    constexpr ImGuiWindowFlags kF =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - kStatusH));
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##Dock", nullptr, kF);
    ImGui::PopStyleVar(3);

    ImGuiID dock = ImGui::GetID("DS");
    ImGui::DockSpace(dock, ImVec2(0, 0), ImGuiDockNodeFlags_None);

    if (m_firstFrame){
        m_firstFrame = false;

        const float vpW = vp->WorkSize.x;
        const float vpH = vp->WorkSize.y - kStatusH;
        const float leftRatio = 220.f / vpW;
        const float rightRatio = 280.f / (vpW - 220.f);
        const float bottomRatio = 200.f / vpH;

        ImGui::DockBuilderRemoveNode(dock);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, ImVec2(vpW, vpH));

        ImGuiID left, mid, right, bottom;
        ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, leftRatio, &left, &mid);
        ImGui::DockBuilderSplitNode(mid, ImGuiDir_Right, rightRatio, &right, &mid);
        ImGui::DockBuilderSplitNode(mid, ImGuiDir_Down, bottomRatio, &bottom, &mid);

        ImGuiID leftTop, leftBot;
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.22f, &leftBot, &leftTop);
        ImGui::DockBuilderDockWindow("Hierarchy", leftTop);
        ImGui::DockBuilderDockWindow("Scene Settings", leftBot);

        ImGui::DockBuilderDockWindow("Viewport", mid);
        ImGui::DockBuilderDockWindow("Game", mid);

        ImGuiID rightTop, rightBot;
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.42f, &rightBot, &rightTop);
        ImGui::DockBuilderDockWindow("Inspector", rightTop);
        ImGui::DockBuilderDockWindow("GPU Memory", rightBot);

        ImGui::DockBuilderDockWindow("Render Graph", bottom);
        ImGui::DockBuilderDockWindow("Asset Browser", bottom);
        ImGui::DockBuilderDockWindow("Collision Debug", bottom);
        ImGui::DockBuilderDockWindow("Console", bottom);
        ImGui::DockBuilderDockWindow("Performance", bottom);
        ImGui::DockBuilderDockWindow("Resources", bottom);

        ImGui::DockBuilderFinish(dock);
    }
    ImGui::End();
}

void ModuleEditor::drawMenuBar(){
    if (!ImGui::BeginMainMenuBar()) return;

    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
        ImGui::Text("Phoenix");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::Text("v0.4");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 8);
    }

    auto saveScene = [&](){
        if (!m_currentScenePath.empty() && m_sceneManager->getActiveScene()){
            bool ok = m_sceneManager->saveCurrentScene(m_currentScenePath);
            log(ok ? "Scene saved!" : "Failed to save.", ok ? EditorColors::Success : EditorColors::Danger);
        }
        else m_saveDialog->open(FileDialog::Type::Save, "Save Scene", "Library/Scenes");
    };

    if (ImGui::BeginMenu("File")){
        if (ImGui::MenuItem("New Scene", "Ctrl+N")) m_showNewSceneConfirm = true;
        if (ImGui::MenuItem("Open Scene", "Ctrl+O")) m_loadDialog->open(FileDialog::Type::Open, "Load Scene", "Library/Scenes/");
        ImGui::Separator();
        if (ImGui::MenuItem("Save Scene", "Ctrl+S")) saveScene();
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) m_saveDialog->open(FileDialog::Type::Save, "Save Scene", "Library/Scenes/");
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")){}
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")){
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo())) undoToSavePoint();
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo())) redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, m_selection.has())) copySelected();
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_clipboard.serialized.empty())) pasteClipboard();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, m_selection.has())) duplicateSelected();
        ImGui::Separator();
        if (ImGui::MenuItem("Create Empty", "Ctrl+Shift+N")) createEmptyGameObject();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("GameObject")){
        if (ImGui::MenuItem("Create Empty")) createEmptyGameObject();
        if (ImGui::MenuItem("Create Empty Child") && m_selection.has()) createEmptyGameObject("Empty", m_selection.object);
        ImGui::Separator();
        if (ImGui::BeginMenu("Primitives")){
            if (ImGui::MenuItem("Cube")) spawnPrimitive(PrimitiveType::Cube);
            if (ImGui::MenuItem("Sphere")) spawnPrimitive(PrimitiveType::Sphere);
            if (ImGui::MenuItem("Capsule")) spawnPrimitive(PrimitiveType::Capsule);
            if (ImGui::MenuItem("Plane")) spawnPrimitive(PrimitiveType::Plane);
            if (ImGui::MenuItem("Cylinder")) spawnPrimitive(PrimitiveType::Cylinder);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Lights")){
            auto spawnLight = [&](const char* name, Component::Type type){
                ModuleScene* sc = getActiveModuleScene();
                if (!sc) return;
                GameObject* go = sc->createGameObject(name);
                go->addComponent(ComponentFactory::CreateComponent(type, go));
                m_selection.object = go;
                log((std::string("Created ") + name).c_str(), EditorColors::Success);
            };
            if (ImGui::MenuItem("Directional Light")) spawnLight("Directional Light", Component::Type::DirectionalLight);
            if (ImGui::MenuItem("Point Light")) spawnLight("Point Light", Component::Type::PointLight);
            if (ImGui::MenuItem("Spot Light")) spawnLight("Spot Light", Component::Type::SpotLight);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Particle Effects")){
            if (ImGui::MenuItem("Fire (Exercise 1)"))
                spawnFireParticleSystem(Vector3(0.f, 0.f, 0.f));
            if (ImGui::MenuItem("Sword Trail"))
                spawnSwordTrail(Vector3(0.f, 0.f, 0.f));
            ImGui::Separator();
            if (ImGui::MenuItem("Fire Comet (Trail + Particles Prefab)"))
                spawnFireComet(Vector3(0.f, 1.5f, 0.f));
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Random Primitive + Physics", "Shift+P")){
            static int menuSpawnIdx = 0; ++menuSpawnIdx;
            static const PrimitiveType kT[] = { PrimitiveType::Cube, PrimitiveType::Sphere, PrimitiveType::Capsule, PrimitiveType::Cylinder };
            spawnPrimitive(kT[menuSpawnIdx % 4],
                Vector3((float)((menuSpawnIdx*3)%11-5), 5.f, (float)((menuSpawnIdx*7)%11-5)),
                Vector3::One, true);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Component")){
        auto addToSel = [&](const char* label, Component::Type type){
            if (!m_selection.has()) return;
            if (ImGui::MenuItem(label)){
                m_selection.object->addComponent(ComponentFactory::CreateComponent(type, m_selection.object));
                log((std::string("Added ") + label).c_str(), EditorColors::Success);
            }
        };
        addToSel("Mesh", Component::Type::Mesh);
        addToSel("Rigidbody", Component::Type::Rigidbody);
        addToSel("Camera", Component::Type::Camera);
        addToSel("Directional Light", Component::Type::DirectionalLight);
        addToSel("Point Light", Component::Type::PointLight);
        addToSel("Spot Light", Component::Type::SpotLight);
        addToSel("Animation", Component::Type::Animation);
        addToSel("Bounds", Component::Type::Bounds);
        addToSel("Decal", Component::Type::Decal);
        addToSel("Billboard", Component::Type::Billboard);
        addToSel("Particle System", Component::Type::ParticleSystem);
        addToSel("Trail", Component::Type::Trail);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Debug")){
        if (m_sceneManager){
            EditorSceneSettings& s = m_sceneManager->getSettings();
            ImGui::MenuItem("AABB Bounding Volumes", nullptr, &s.debugDrawBounds);
            ImGui::MenuItem("Broadphase Grid", nullptr, &s.debugDrawGrid);
            ImGui::MenuItem("Show Light Proxies", nullptr, &s.debugDrawLights);
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Camera & Culling")){
            ModuleCamera* cam = app->getCamera();
            if (cam){
                ImGui::Text("Active Game Camera");
                GameObject* activeCamGO = cam->getActiveCamera();
                const char* preview = activeCamGO ? activeCamGO->getName().c_str() : "(none)";
                if (ImGui::BeginCombo("##ActiveGameCamera", preview)){
                    if (ImGui::Selectable("(none)", activeCamGO == nullptr)){
                        cam->setActiveCamera(nullptr);
                        cam->clearGameCameraFrustum();
                    }
                    if (ModuleScene* scene = getActiveModuleScene()){
                        std::function<void(GameObject*)> listCams = [&](GameObject* node){
                            if (!node) return;
                            if (node->getComponent<ComponentCamera>()){
                                bool selected = (node == activeCamGO);
                                if (ImGui::Selectable(node->getName().c_str(), selected))
                                    cam->setActiveCamera(node);
                            }
                            for (auto* child : node->getChildren()) listCams(child);
                        };
                        listCams(scene->getRoot());
                    }
                    ImGui::EndCombo();
                }
                ImGui::Separator();

                int cm = (int)cam->cullMode;
                ImGui::Text("Cull Mode"); ImGui::SameLine();
                if (ImGui::RadioButton("Off##cm", &cm, 0)) cam->cullMode = ModuleCamera::CullMode::None;
                ImGui::SameLine();
                if (ImGui::RadioButton("Frustum##cm", &cm, 1)) cam->cullMode = ModuleCamera::CullMode::Frustum;
                int cs = (int)cam->cullSource;
                ImGui::Text("Cull From"); ImGui::SameLine();
                if (ImGui::RadioButton("Editor##cs", &cs, 0)) cam->cullSource = ModuleCamera::CullSource::EditorCamera;
                ImGui::SameLine();
                if (ImGui::RadioButton("Game##cs", &cs, 1)) cam->cullSource = ModuleCamera::CullSource::GameCamera;
                ImGui::Separator();
                ImGui::Text("Debug Draw");
                ImGui::MenuItem("Editor Frustum", nullptr, &cam->debugDrawEditorFrustum);
                ImGui::MenuItem("Cull Frustum", nullptr, &cam->debugDrawCullFrustum);
                ImGui::MenuItem("Camera Axes", nullptr, &cam->debugDrawCameraAxes);
                ImGui::MenuItem("Forward Ray", nullptr, &cam->debugDrawForwardRay);
                ImGui::Separator();
                ImGui::Text("Camera Parameters");
                float fovDeg = cam->fovY * 57.2957795f;
                ImGui::SetNextItemWidth(140.f);
                if (ImGui::SliderFloat("FOV##cam", &fovDeg, 10.f, 170.f)) cam->fovY = fovDeg * 0.0174532925f;
                ImGui::SetNextItemWidth(140.f);
                ImGui::DragFloat("Near##cam", &cam->nearZ, 0.01f, 0.01f, 10.f);
                ImGui::SetNextItemWidth(140.f);
                ImGui::DragFloat("Far##cam", &cam->farZ, 1.f, 10.f, 5000.f);
                ImGui::SetNextItemWidth(140.f);
                ImGui::SliderFloat("Aspect##cam", &cam->aspectRatio, 0.5f, 4.f);
                ImGui::Separator();
                Vector3 fwd = cam->getForward();
                ImGui::TextDisabled("Pos: %.2f  %.2f  %.2f", cam->getPos().x, cam->getPos().y, cam->getPos().z);
                ImGui::TextDisabled("Fwd: %.2f  %.2f  %.2f", fwd.x, fwd.y, fwd.z);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")){
        for (EditorPanel* p : m_panels){
            const char* n = p->getName();
            if (strcmp(n,"Render Graph")==0 || strcmp(n,"GPU Memory")==0 ||
                strcmp(n,"Collision Debug")==0 || strcmp(n,"Performance")==0) continue;
            ImGui::MenuItem(n, nullptr, &p->open);
        }
        ImGui::Separator();
        ImGui::SeparatorText("PROFILING");
        for (EditorPanel* p : m_panels){
            const char* n = p->getName();
            if (strcmp(n,"Render Graph")==0 || strcmp(n,"GPU Memory")==0 ||
                strcmp(n,"Collision Debug")==0 || strcmp(n,"Performance")==0)
                ImGui::MenuItem(n, nullptr, &p->open);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) m_firstFrame = true;
        ImGui::EndMenu();
    }

    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        char gpuInfo[128];
        snprintf(gpuInfo, sizeof(gpuInfo), "RTX \xC2\xB7 build Development    %.0f fps",
            (double)app->getFPS());
        float textW = ImGui::CalcTextSize(gpuInfo).x;
        float rightX = ImGui::GetWindowWidth() - textW - 14.f;
        if (rightX > ImGui::GetCursorPosX())
            ImGui::SetCursorPosX(rightX);
        ImGui::TextUnformatted(gpuInfo);
        ImGui::PopStyleColor();
    }

    ImGui::EndMainMenuBar();
}

void ModuleEditor::drawStatusBar(){
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - kStatusH));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, kStatusH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 3.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.048f, 0.048f, 0.060f, 1.f));
    constexpr ImGuiWindowFlags kF =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoScrollbar;
    if (!ImGui::Begin("##StatusBar", nullptr, kF)){
        ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar(3); return;
    }

    ImGui::PushFont(g_fontMono);

    {
        ImVec2 dp = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImVec2(dp.x + 5.f, dp.y + 8.f), 4.f,
            ImGui::ColorConvertFloat4ToU32(EditorColors::Ok));
        ImGui::Dummy(ImVec2(14.f, 0.f)); ImGui::SameLine(0, 0);

        const char* sceneName = "Untitled";
        static char sceneNameBuf[128];
        if (m_sceneManager && !m_currentScenePath.empty()){
            snprintf(sceneNameBuf, sizeof(sceneNameBuf), "%s",
                std::filesystem::path(m_currentScenePath).filename().string().c_str());
            sceneName = sceneNameBuf;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
        ImGui::Text("Ready  \xC2\xB7  Scene: %s", sceneName);
        ImGui::PopStyleColor();
        if (m_selection.has()){
            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
            ImGui::Text("  \xC2\xB7  1 selected  \xC2\xB7  ");
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
            ImGui::TextUnformatted(m_selection.object->getName().c_str());
            ImGui::PopStyleColor();
        }
    }

    {
        ModuleD3D12* d3d = app->getD3D12();
        ModuleStaticBuffer* sb = app->getStaticBuffer();
        SIZE_T vramBytes = d3d ? d3d->getDedicatedVideoMemory() : 0;
        float vramUsed = sb ? (float)sb->getUsedBytes() / (1024.f*1024.f*1024.f) : 0.f;
        float vramTotal = vramBytes > 0
            ? (float)vramBytes / (1024.f*1024.f*1024.f)
            : (sb ? (float)sb->getTotalBytes() / (1024.f*1024.f*1024.f) : 0.f);

        char main[192], vram[48];
        snprintf(main, sizeof(main),
            "Renderer: D3D12  \xC2\xB7  Passes: 9  \xC2\xB7  Draw Calls: %d  \xC2\xB7  ",
            m_frameDrawCalls);
        if (vramTotal > 0.f) snprintf(vram, sizeof(vram), "VRAM: %.2f / %.1f GB", vramUsed, vramTotal);
        else snprintf(vram, sizeof(vram), "VRAM: —");

        float mainW = ImGui::CalcTextSize(main).x;
        float vramW = ImGui::CalcTextSize(vram).x;
        float totalW = mainW + vramW + 4.f;

        ImVec2 wp = ImGui::GetWindowPos();
        float wW = ImGui::GetWindowWidth();
        float textY = wp.y + (kStatusH - ImGui::GetTextLineHeight()) * 0.5f;
        float rx = wp.x + wW - totalW - 8.f;

        ImDrawList* ddl = ImGui::GetWindowDrawList();
        ddl->AddText(ImVec2(rx, textY),
            ImGui::ColorConvertFloat4ToU32(EditorColors::Tx2), main);
        ddl->AddText(ImVec2(rx + mainW + 4.f, textY),
            ImGui::ColorConvertFloat4ToU32(EditorColors::Acc), vram);
    }

    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void ModuleEditor::handleDialogs(){
    auto tryScene = [&](bool ok, const char* good, const char* bad){ log(ok ? good : bad, ok ? EditorColors::Success : EditorColors::Danger); };
    if (m_saveDialog->draw() && m_sceneManager->getActiveScene()){
        const std::string& p = m_saveDialog->getSelectedPath();
        if (m_sceneManager->saveCurrentScene(p)){
            m_currentScenePath = p;
            m_savePointIndex = (int)m_undoStack.size();
            m_redoStack.clear();
            tryScene(true, "Scene saved!", "");
        }
        else tryScene(false, "", "Failed to save scene.");
    }
    if (m_loadDialog->draw() && m_sceneManager->getActiveScene()){
        const std::string& p = m_loadDialog->getSelectedPath();
        if (m_sceneManager->loadScene(p)){ m_currentScenePath = p; tryScene(true, "Scene loaded!", ""); }
        else tryScene(false, "", "Failed to load scene.");
    }
}

void ModuleEditor::handleNewScenePopup(ID3D12GraphicsCommandList*){
    if (m_showNewSceneConfirm){ ImGui::OpenPopup("New Scene?"); m_showNewSceneConfirm = false; }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal("New Scene?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::Text("This will clear the current scene.");
    ImGui::TextColored(EditorColors::Warning, "Unsaved changes will be lost!");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    if (ImGui::Button("Create New Scene", ImVec2(160, 0))){
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

GameObject* ModuleEditor::createEmptyGameObject(const char* name, GameObject* parent){
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
        [this, serialized, livePtr, goName](){
            ModuleScene* s = getActiveModuleScene();
            if (!s) return;
            GameObject* restored = serialized->empty()
                ? s->createGameObject(goName.c_str())
                : PrefabManager::deserializeGameObject(*serialized, s);
            if (restored){ *livePtr = restored; m_selection.object = restored; log(("Redo create: " + goName).c_str(), EditorColors::Success); }
        },
        [this, livePtr, serialized, goName](){
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

void ModuleEditor::deleteGameObject(GameObject* go){
    if (!go) return;
    if (m_selection.object == go || isChildOf(go, m_selection.object)) m_selection.clear();
    ModuleScene* scene = getActiveModuleScene();
    if (!scene) return;

    std::vector<GameObject*> subtree;
    {
        std::function<void(GameObject*)> collect = [&](GameObject* node){
            subtree.push_back(node);
            for (auto* c : node->getChildren()) collect(c);
        };
        collect(go);
    }

    if (ModuleCamera* cam = app->getCamera()){
        GameObject* active = cam->getActiveCamera();
        if (active && (active == go || isChildOf(go, active))){
            cam->setActiveCamera(nullptr);
            cam->clearGameCameraFrustum();
        }
    }

    {
        std::function<void(GameObject*)> scanMeshes = [&](GameObject* node){
            if (!node) return;
            if (auto* cm = node->getComponent<ComponentMesh>()){
                for (GameObject* doomed : subtree) cm->nullifyJoint(doomed);
            }
            for (auto* c : node->getChildren()) scanMeshes(c);
        };
        scanMeshes(scene->getRoot());
    }

    std::string name = go->getName();

    app->getD3D12()->flush();
    for (int i = (int)subtree.size() - 1; i >= 0; --i)
        scene->destroyGameObject(subtree[i]);

    log(("Deleted: " + name).c_str(), EditorColors::Warning);

    pushCommand({
        [](){},
        [](){}
    });
}

void ModuleEditor::spawnAssetAtPath(const std::string& path){
    if (path.empty() || !fs::exists(path) || fs::is_directory(path)) return;
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    ModuleScene* scene = getActiveModuleScene();
    if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj"){
        if (!scene) return;
        if (GameObject* go = spawnModel(path)) m_selection.object = go;
    }
    else if (ext == ".json"){
        if (m_sceneManager && m_sceneManager->loadScene(path)) log(("Loaded scene: " + path).c_str(), EditorColors::Success);
    }
    else if (ext == ".prefab"){
        if (!scene) return;
        std::string stem = fs::path(path).stem().string();
        PrefabManager::instantiatePrefab(stem, scene);
        log(("Instantiated: " + stem).c_str(), EditorColors::Success);
    }
}

GameObject* ModuleEditor::spawnModel(const std::string& path){
    ModuleScene* scene = getActiveModuleScene();
    if (!scene) return nullptr;

    std::string stem = fs::path(path).stem().string();

    UID uid = app->getAssets()->findUID(path);
    if (uid == 0){
        log(("Cannot find UID for: " + path).c_str(), EditorColors::Danger);
        return nullptr;
    }

    ResourceModel* model = app->getResources()->RequestModel(uid);
    if (model){
        int meshNodeCount = 0;
        for (const auto& n : model->getNodes())
            if (!n.meshes.empty()) ++meshNodeCount;

        bool hasAnimations = (app->getAssets()->findSubUID(path, "anim", 0) != 0);
        bool hasSkin = !model->getSkins().empty();
        bool needsHierarchy = meshNodeCount > 1 || hasSkin || hasAnimations;
        if (needsHierarchy){
            GameObject* root = model->spawnIntoScene(scene);
            app->getResources()->ReleaseResource(model);
            if (root){
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

    GameObject* go = scene->createGameObject(stem);
    bool ok = go->createComponent<ComponentMesh>()->loadModel(path.c_str());
    log(ok ? ("Added: " + stem).c_str() : ("Failed: " + path).c_str(),
        ok ? EditorColors::Success : EditorColors::Danger);
    return ok ? go : nullptr;
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

void ModuleEditor::debugDrawLights(ModuleScene* scene, float sz){
    if (!scene) return;
    auto v = [](const Vector3& x) -> const float* { return &x.x; };
    std::function<void(GameObject*)> visit = [&](GameObject* node){
        if (!node || !node->isActive()) return;
        if (auto* dl = node->getComponent<ComponentDirectionalLight>(); dl && dl->enabled){
            Vector3 p = node->getTransform()->getGlobalMatrix().Translation();
            Vector3 d = dl->direction; d.Normalize();
            float h = sz * .2f;
            dd::line(v(p), v(p + d * sz * 2.f), dd::colors::Yellow);
            dd::line(v(p - Vector3(h, 0, 0)), v(p + Vector3(h, 0, 0)), dd::colors::Yellow);
            dd::line(v(p - Vector3(0, h, 0)), v(p + Vector3(0, h, 0)), dd::colors::Yellow);
            dd::line(v(p - Vector3(0, 0, h)), v(p + Vector3(0, 0, h)), dd::colors::Yellow);
        }
        if (auto* pl = node->getComponent<ComponentPointLight>(); pl && pl->enabled){
            Vector3 p = node->getTransform()->getGlobalMatrix().Translation();
            float h = sz * .2f;
            dd::sphere(v(p), dd::colors::Cyan, pl->radius);
            dd::line(v(p - Vector3(h, 0, 0)), v(p + Vector3(h, 0, 0)), dd::colors::Cyan);
            dd::line(v(p - Vector3(0, h, 0)), v(p + Vector3(0, h, 0)), dd::colors::Cyan);
            dd::line(v(p - Vector3(0, 0, h)), v(p + Vector3(0, 0, h)), dd::colors::Cyan);
        }
        if (auto* sl = node->getComponent<ComponentSpotLight>(); sl && sl->enabled){
            Vector3 p = node->getTransform()->getGlobalMatrix().Translation();
            Vector3 dir = sl->direction; dir.Normalize();
            float outerR = tanf(sl->outerAngle * kDeg2Rad) * sl->radius;
            Vector3 tip = p + dir * sl->radius;
            Vector3 up = (fabsf(dir.y) < .99f) ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
            Vector3 right = dir.Cross(up); right.Normalize();
            up = right.Cross(dir); up.Normalize();
            const int segs = 8;
            for (int i = 0; i < segs; ++i){
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

ImVec2 ModuleEditor::getSceneViewSize() const{
    return m_sceneView ? m_sceneView->viewport.size : ImVec2(0, 0);
}

void ModuleEditor::pushCommand(EditorCommand cmd){
    m_redoStack.clear();
    m_undoStack.push_back(std::move(cmd));
    if ((int)m_undoStack.size() > kMaxUndoSteps){
        m_undoStack.pop_front();
        if (m_savePointIndex > 0) --m_savePointIndex;
    }
}

bool ModuleEditor::canUndo() const { return (int)m_undoStack.size() > m_savePointIndex; }
bool ModuleEditor::canRedo() const { return !m_redoStack.empty(); }

void ModuleEditor::undoToSavePoint(){
    if (!canUndo()) return;
    EditorCommand& cmd = m_undoStack.back();
    cmd.undo();
    m_redoStack.push_back(std::move(cmd));
    m_undoStack.pop_back();
}

void ModuleEditor::redo(){
    if (!canRedo()) return;
    EditorCommand& cmd = m_redoStack.back();
    cmd.execute();
    m_undoStack.push_back(std::move(cmd));
    m_redoStack.pop_back();
}

void ModuleEditor::copySelected(){
    if (!m_selection.has()) return;
    m_clipboard.name = m_selection.object->getName();
    m_clipboard.serialized = PrefabManager::serializeGameObject(m_selection.object);
    log(("Copied: " + m_clipboard.name).c_str(), EditorColors::Info);
}

void ModuleEditor::pasteClipboard(){
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
        [this, clipData, livePtr, pastedName](){
            ModuleScene* s = getActiveModuleScene();
            if (!s) return;
            GameObject* restored = PrefabManager::deserializeGameObject(clipData, s);
            if (restored){ *livePtr = restored; m_selection.object = restored; log(("Redo paste: " + pastedName).c_str(), EditorColors::Success); }
        },
        [this, livePtr, pastedName](){
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

void ModuleEditor::duplicateSelected(){
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
        [this, serialized, livePtr, dupeName](){
            ModuleScene* s = getActiveModuleScene();
            if (!s) return;
            GameObject* restored = PrefabManager::deserializeGameObject(serialized, s);
            if (restored){ *livePtr = restored; m_selection.object = restored; log(("Redo duplicate: " + dupeName).c_str(), EditorColors::Success); }
        },
        [this, livePtr, dupeName](){
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

GameObject* ModuleEditor::spawnPrimitive(PrimitiveType type,
                                          const Vector3& position,
                                          const Vector3& scale,
                                          bool addPhysics){
    ModuleScene* scene = getActiveModuleScene();
    if (!scene) return nullptr;

    static const char* kNames[] = { "Cube","Sphere","Capsule","Plane","Cylinder" };
    int ti = static_cast<int>(type);
    const char* name = (ti >= 0 && ti < 5) ? kNames[ti] : "Primitive";

    std::unique_ptr<Mesh> mesh;
    switch (type){
    case PrimitiveType::Cube: mesh = PrimitiveFactory::createCubeMesh(); break;
    case PrimitiveType::Sphere: mesh = PrimitiveFactory::createSphereMesh(); break;
    case PrimitiveType::Capsule: mesh = PrimitiveFactory::createCapsuleMesh(); break;
    case PrimitiveType::Plane: mesh = PrimitiveFactory::createPlaneMesh(); break;
    case PrimitiveType::Cylinder: mesh = PrimitiveFactory::createCylinderMesh(); break;
    default: return nullptr;
    }

    GameObject* go = scene->createGameObject(name);
    auto* t = go->getTransform();
    t->position = position;
    t->scale = scale;
    t->markDirty();

    auto* cm = go->createComponent<ComponentMesh>();
    cm->setProceduralModel(PrimitiveFactory::meshToModel(std::move(mesh)));

    if (type == PrimitiveType::Sphere){
        auto* cb = go->createComponent<ComponentBounds>();
        cb->bvType = BVType::Sphere;
    }

    if (addPhysics){
        auto* rb = go->createComponent<ComponentRigidbody>();
        rb->mass = 1.f;
        rb->useGravity = true;
        rb->restitution = 0.5f;
        rb->linearDamping = 0.1f;
    }

    m_selection.object = go;
    log(std::string("Spawned ").append(name).c_str(), EditorColors::Success);
    return go;
}

GameObject* ModuleEditor::spawnFireParticleSystem(const Vector3& position){
    ModuleScene* scene = getActiveModuleScene();
    if (!scene) return nullptr;

    const std::string fireTex = "Library/Textures/TXT_Fire_01.dds";
    const std::string sparksTex = "Library/Textures/TXT_Sparks_01.dds";

    GameObject* root = scene->createGameObject("Fire (Exercise 1)");
    root->getTransform()->position = position;
    root->getTransform()->markDirty();

    {
        GameObject* go = scene->createGameObject("Flames", root);
        auto* ps = go->createComponent<ComponentParticleSystem>();
        ps->shape = ComponentParticleSystem::EmitterShape::Cone;
        ps->shapeRadius = 0.35f;
        ps->coneAngleDeg = 18.f;
        ps->emissionRate = 30.f;
        ps->maxParticles = 200;
        ps->lifeRange = Vector2(0.8f, 1.4f);
        ps->speedRange = Vector2(0.8f, 1.6f);
        ps->sizeRange = Vector2(0.4f, 0.8f);
        ps->rotationRange = Vector2(-45.f, 45.f);
        ps->startColor = Vector4(1.f, 0.95f, 0.6f, 1.f);
        ps->endColor = Vector4(1.f, 0.25f, 0.05f, 0.f);
        ps->startSizeMul = 0.6f;
        ps->endSizeMul = 1.4f;
        ps->texturePath = fireTex;
        ps->sheetColumns = 2;
        ps->sheetRows = 2;
        ps->randomFrame = true;
        ps->blendMode = ComponentParticleSystem::BlendMode::Alpha;
        ps->layer = 0;
    }

    {
        GameObject* go = scene->createGameObject("Flames Inner Light", root);
        auto* ps = go->createComponent<ComponentParticleSystem>();
        ps->shape = ComponentParticleSystem::EmitterShape::Cone;
        ps->shapeRadius = 0.2f;
        ps->coneAngleDeg = 14.f;
        ps->emissionRate = 24.f;
        ps->maxParticles = 150;
        ps->lifeRange = Vector2(0.6f, 1.0f);
        ps->speedRange = Vector2(0.8f, 1.5f);
        ps->sizeRange = Vector2(0.18f, 0.35f);
        ps->rotationRange = Vector2(-45.f, 45.f);
        ps->startColor = Vector4(1.f, 1.f, 0.85f, 1.f);
        ps->endColor = Vector4(1.f, 0.5f, 0.1f, 0.f);
        ps->startSizeMul = 0.7f;
        ps->endSizeMul = 1.2f;
        ps->texturePath = fireTex;
        ps->sheetColumns = 2;
        ps->sheetRows = 2;
        ps->randomFrame = true;
        ps->blendMode = ComponentParticleSystem::BlendMode::Additive;
        ps->layer = 1;
    }

    {
        GameObject* go = scene->createGameObject("Fire Glow", root);
        auto* ps = go->createComponent<ComponentParticleSystem>();
        ps->shape = ComponentParticleSystem::EmitterShape::Cone;
        ps->shapeRadius = 0.4f;
        ps->coneAngleDeg = 10.f;
        ps->emissionRate = 8.f;
        ps->maxParticles = 60;
        ps->lifeRange = Vector2(1.2f, 2.0f);
        ps->speedRange = Vector2(0.2f, 0.5f);
        ps->sizeRange = Vector2(1.0f, 1.6f);
        ps->rotationRange = Vector2(-30.f, 30.f);
        ps->startColor = Vector4(1.f, 0.6f, 0.2f, 0.35f);
        ps->endColor = Vector4(1.f, 0.3f, 0.05f, 0.f);
        ps->startSizeMul = 0.5f;
        ps->endSizeMul = 1.8f;
        ps->texturePath = fireTex;
        ps->sheetColumns = 1;
        ps->sheetRows = 1;
        ps->randomFrame = false;
        ps->blendMode = ComponentParticleSystem::BlendMode::Additive;
        ps->layer = 2;
    }

    {
        GameObject* go = scene->createGameObject("Sparks", root);
        auto* ps = go->createComponent<ComponentParticleSystem>();
        ps->shape = ComponentParticleSystem::EmitterShape::Cone;
        ps->shapeRadius = 0.15f;
        ps->coneAngleDeg = 30.f;
        ps->emissionRate = 15.f;
        ps->maxParticles = 100;
        ps->lifeRange = Vector2(0.4f, 0.9f);
        ps->speedRange = Vector2(2.0f, 4.5f);
        ps->sizeRange = Vector2(0.04f, 0.10f);
        ps->rotationRange = Vector2(0.f, 0.f);
        ps->startColor = Vector4(1.f, 0.95f, 0.6f, 1.f);
        ps->endColor = Vector4(1.f, 0.35f, 0.05f, 0.f);
        ps->startSizeMul = 1.f;
        ps->endSizeMul = 0.3f;
        ps->gravity = Vector3(0.f, -1.5f, 0.f);
        ps->texturePath = sparksTex;
        ps->sheetColumns = 1;
        ps->sheetRows = 1;
        ps->randomFrame = false;
        ps->blendMode = ComponentParticleSystem::BlendMode::Additive;
        ps->layer = 3;

        ps->useTurbulence = true;
        ps->turbulenceFrequency = 0.6f;
        ps->turbulenceStrength = 1.2f;
        ps->turbulenceOctaves = 3;
        ps->turbulenceScroll = 0.4f;
    }

    m_selection.object = root;
    log("Spawned Fire (Exercise 1): using TXT_Fire_01 + TXT_Sparks_01 textures", EditorColors::Success);
    return root;
}

GameObject* ModuleEditor::spawnSwordTrail(const Vector3& position){
    ModuleScene* scene = getActiveModuleScene();
    if (!scene) return nullptr;

    const std::string swordModel = "Assets/Models/Sword/sword.gltf";
    const std::string swooshTex = "Assets/Models/Sword/swoosh.png";

    GameObject* root = scene->createGameObject("Sword Trail");
    {
        auto* t = root->getTransform();
        t->position = position;
        t->markDirty();
    }

    {
        GameObject* swordGO = scene->createGameObject("Sword", root);
        auto* t = swordGO->getTransform();
        t->position = Vector3(0.f, 0.f, 0.f);
        t->scale = Vector3(1.f, 1.f, 1.f);
        t->markDirty();

        auto* cm = swordGO->createComponent<ComponentMesh>();
        if (!cm->loadModel(swordModel.c_str()))
            LOG("spawnSwordTrail: could not load '%s'", swordModel.c_str());
    }

    {
        auto* tr = root->createComponent<ComponentTrail>();
        tr->enabled = true;
        tr->emitting = true;
        tr->duration = 0.45f;
        tr->minPointDistance = 0.02f;
        tr->width = 0.55f;

        tr->useCatmullRom = true;
        tr->catmullRomAlpha = 0.5f;
        tr->subdivisions = 10;

        tr->startColor = Vector4(1.f, 1.f, 0.85f, 0.95f);
        tr->endColor = Vector4(1.f, 0.55f, 0.1f, 0.0f);
        tr->startWidthMul = 1.0f;
        tr->endWidthMul = 0.05f;

        tr->texturePath = swooshTex;
        tr->blendMode = ComponentTrail::BlendMode::Additive;
        tr->textureMode = ComponentTrail::TextureMode::Stretch;
        tr->layer = 0;
    }

    m_selection.object = root;
    log("Spawned Sword Trail: sword.gltf + swoosh.png", EditorColors::Success);
    return root;
}

GameObject* ModuleEditor::spawnFireComet(const Vector3& position){
    ModuleScene* scene = getActiveModuleScene();
    if (!scene) return nullptr;

    const std::string fireTex = "Library/Textures/TXT_Fire_01.dds";
    const std::string sparksTex = "Library/Textures/TXT_Sparks_01.dds";

    GameObject* root = scene->createGameObject("Fire Comet");
    {
        auto* t = root->getTransform();
        t->position = position;
        t->markDirty();
    }

    {
        auto* tr = root->createComponent<ComponentTrail>();
        tr->enabled = true;
        tr->emitting = true;
        tr->duration = 1.2f;
        tr->minPointDistance = 0.015f;
        tr->width = 0.8f;
        tr->maxSegmentAngle = 15.f;

        tr->useCatmullRom = true;
        tr->catmullRomAlpha = 0.5f;
        tr->subdivisions = 10;

        tr->startColor = Vector4(0.30f, 0.70f, 1.0f, 1.0f);
        tr->endColor = Vector4(0.10f, 1.0f, 0.60f, 0.0f);
        tr->startWidthMul = 2.0f;
        tr->endWidthMul = 0.0f;

        tr->texturePath = "";
        tr->blendMode = ComponentTrail::BlendMode::Additive;
        tr->textureMode = ComponentTrail::TextureMode::Stretch;
        tr->layer = 0;

        tr->previewOrbit = true;
        tr->orbitRadius = 1.5f;
        tr->orbitSpeed = 2.5f;
    }

    {
        GameObject* flameGO = scene->createGameObject("Core Flames", root);
        flameGO->getTransform()->markDirty();

        auto* ps = flameGO->createComponent<ComponentParticleSystem>();
        ps->enabled = true;
        ps->playing = true;
        ps->looping = true;
        ps->emissionRate = 75.f;
        ps->maxParticles = 160;

        ps->shape = ComponentParticleSystem::EmitterShape::Cone;
        ps->shapeRadius = 0.06f;
        ps->coneAngleDeg = 18.f;
        ps->worldSpace = true;

        ps->lifeRange = Vector2(0.20f, 0.45f);
        ps->speedRange = Vector2(0.5f, 1.4f);
        ps->sizeRange = Vector2(0.13f, 0.22f);
        ps->rotationRange = Vector2(-30.f, 30.f);

        ps->startColor = Vector4(1.00f, 0.85f, 0.30f, 1.00f);
        ps->endColor = Vector4(0.90f, 0.12f, 0.00f, 0.00f);
        ps->startSizeMul = 1.0f;
        ps->endSizeMul = 0.05f;

        ps->gravity = Vector3(0.f, 0.35f, 0.f);
        ps->useTurbulence = false;

        ps->texturePath = fireTex;
        ps->sheetColumns = 2;
        ps->sheetRows = 2;
        ps->randomFrame = true;
        ps->blendMode = ComponentParticleSystem::BlendMode::Alpha;
    }

    {
        GameObject* emberGO = scene->createGameObject("Embers", root);
        emberGO->getTransform()->markDirty();

        auto* ps = emberGO->createComponent<ComponentParticleSystem>();
        ps->enabled = true;
        ps->playing = true;
        ps->looping = true;
        ps->emissionRate = 32.f;
        ps->maxParticles = 80;

        ps->shape = ComponentParticleSystem::EmitterShape::Sphere;
        ps->shapeRadius = 0.10f;
        ps->worldSpace = true;

        ps->lifeRange = Vector2(0.55f, 1.20f);
        ps->speedRange = Vector2(0.25f, 0.75f);
        ps->sizeRange = Vector2(0.035f, 0.065f);
        ps->rotationRange = Vector2(-180.f, 180.f);

        ps->startColor = Vector4(1.00f, 0.72f, 0.10f, 1.00f);
        ps->endColor = Vector4(0.85f, 0.10f, 0.00f, 0.00f);
        ps->startSizeMul = 1.0f;
        ps->endSizeMul = 0.0f;

        ps->gravity = Vector3(0.f, -0.40f, 0.f);
        ps->useTurbulence = true;
        ps->turbulenceFrequency = 1.5f;
        ps->turbulenceStrength = 1.2f;
        ps->turbulenceOctaves = 3;
        ps->turbulenceScroll = 0.5f;

        ps->texturePath = sparksTex;
        ps->sheetColumns = 1;
        ps->sheetRows = 1;
        ps->randomFrame = false;
        ps->blendMode = ComponentParticleSystem::BlendMode::Additive;
    }

    if (PrefabManager::createPrefab(root, "FireComet"))
        log("FireComet prefab saved — instantiate any time from Asset Browser", EditorColors::Success);

    m_effectsPlaying = true;
    m_effectsTime = 0.f;

    m_selection.object = root;
    log("Spawned Fire Comet: orbiting in edit mode — trail + flames + embers live (disable Preview Orbit in Inspector before Play)", EditorColors::Success);
    return root;
}


template<typename TrailFn, typename PsFn>
static void forEachEffect(GameObject* root, TrailFn trailFn, PsFn psFn){
    if (!root) return;
    std::function<void(GameObject*)> visit = [&](GameObject* go){
        if (!go || !go->isActive()) return;
        if (auto* tr = go->getComponent<ComponentTrail>()) trailFn(tr);
        if (auto* ps = go->getComponent<ComponentParticleSystem>()) psFn(ps);
        for (auto* c : go->getChildren()) visit(c);
    };
    visit(root);
}

void ModuleEditor::effectsStop(){
    m_effectsPlaying = false;
    m_effectsTime = 0.f;
    ModuleScene* ms = getActiveModuleScene();
    if (!ms) return;
    forEachEffect(ms->getRoot(),
        [](ComponentTrail* tr){ tr->clear(); },
        [](ComponentParticleSystem* ps){ ps->clear(); });
}

void ModuleEditor::effectsRestartAll(){
    ModuleScene* ms = getActiveModuleScene();
    if (!ms) return;
    forEachEffect(ms->getRoot(),
        [](ComponentTrail* tr){ tr->clear(); },
        [](ComponentParticleSystem* ps){ ps->clear(); });
    m_effectsPlaying = true;
    m_effectsTime = 0.f;
    log("Effects restarted (all)", EditorColors::Info);
}

void ModuleEditor::effectsRestartSelected(){
    if (m_selection.has()){
        bool did = false;
        if (auto* tr = m_selection.object->getComponent<ComponentTrail>())
            { tr->clear(); did = true; }
        if (auto* ps = m_selection.object->getComponent<ComponentParticleSystem>())
            { ps->clear(); did = true; }
        if (did){
            m_effectsPlaying = true;
            m_effectsTime = 0.f;
            log("Effects restarted (selected)", EditorColors::Info);
            return;
        }
    }
    effectsRestartAll();
}

void ModuleEditor::updateEffectsInEditMode(float dt){
    ModuleScene* ms = getActiveModuleScene();
    if (!ms) return;
    m_effectsTime += dt;
    std::function<void(GameObject*)> visit = [&](GameObject* go){
        if (!go || !go->isActive()) return;
        if (auto* tr = go->getComponent<ComponentTrail>()) tr->update(dt);
        if (auto* ps = go->getComponent<ComponentParticleSystem>()) ps->update(dt);
        for (auto* c : go->getChildren()) visit(c);
    };
    visit(ms->getRoot());
}

void ModuleEditor::stopPlay(){
    if (m_sceneManager) m_sceneManager->stop();
    m_selection.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_savePointIndex = 0;
}


void ModuleEditor::handleShortcuts(){
    if (ImGui::GetIO().WantTextInput) return;
    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl;
    bool shift = io.KeyShift;
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_N, false)) m_showNewSceneConfirm = true;
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_N, false)) createEmptyGameObject();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_S, false)){
        if (!m_currentScenePath.empty() && m_sceneManager->getActiveScene()){
            bool ok = m_sceneManager->saveCurrentScene(m_currentScenePath);
            if (ok){ m_savePointIndex = (int)m_undoStack.size(); m_redoStack.clear(); }
            log(ok ? "Scene saved!" : "Failed to save.", ok ? EditorColors::Success : EditorColors::Danger);
        }
        else m_saveDialog->open(FileDialog::Type::Save, "Save Scene", "Library/Scenes");
    }
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_S, false)) m_saveDialog->open(FileDialog::Type::Save, "Save Scene", "Library/Scenes/");
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) m_loadDialog->open(FileDialog::Type::Open, "Load Scene", "Library/Scenes/");
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && m_selection.has()) deleteGameObject(m_selection.object);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && m_sceneManager && m_sceneManager->isEditingPrefab()){ exitPrefabEdit(); return; }
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) undoToSavePoint();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Y, false)) redo();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_C, false)) copySelected();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_V, false)) pasteClipboard();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_D, false)) duplicateSelected();

    if (!ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_P, false)){
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

void ModuleEditor::enterPrefabEdit(const std::string& prefabName){
    if (!m_sceneManager) return;
    app->getD3D12()->flush();
    if (m_sceneManager->isEditingPrefab()) m_sceneManager->exitPrefabEdit();
    m_prefabSession.clear();
    m_prefabSession.isolatedScene = std::make_unique<ModuleScene>();
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

static void drawDashedRect(ImDrawList* dl, ImVec2 a, ImVec2 b,
                            ImU32 col, float thick, float dash, float gap){
    auto seg = [&](ImVec2 p0, ImVec2 p1){
        float dx = p1.x - p0.x, dy = p1.y - p0.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.001f) return;
        float nx = dx / len, ny = dy / len, t = 0.f;
        bool on = true;
        while (t < len){
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

    if (ddm.IsDragging()){
        fg->AddRectFilled({ 0.f, 0.f }, dsz, IM_COL32(0, 0, 0, 100));

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

    const DragDropManager::ImportProgress prog = ddm.GetProgress();
    if (prog.active || prog.showComplete){
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

        if (ImGui::Begin("##DDProgressModal", nullptr, kFlags)){
            if (prog.active){
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

            if (!prog.completedFiles.empty()){
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::BeginChild("##DDDoneList", { 0.f, 0.f }, false,
                                  ImGuiWindowFlags_NoScrollbar);
                for (const auto& cf : prog.completedFiles){
                    ImGui::TextColored({ 0.45f, 0.88f, 0.52f, 1.f },
                                       "\xe2\x9c\x93 %s", cf.c_str());
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }
}
