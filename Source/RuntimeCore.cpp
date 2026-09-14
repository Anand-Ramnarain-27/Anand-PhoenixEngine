#include "Globals.h"
#include "RuntimeCore.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleFileSystem.h"
#include "ModuleStaticBuffer.h"
#include "ModuleResources.h"
#include "BuildSettings.h"
#include "DebugDrawPass.h"
#include "ComponentDecal.h"
#include "ComponentBillboard.h"
#include "ComponentParticleSystem.h"
#include "ComponentTrail.h"
#include "ComponentCamera.h"
#include "ResourceMaterial.h"
#include "RenderTexture.h"
#include "EditorViewport.h"
#include "EmptyScene.h"
#include "SceneGraph.h"
#include "SceneManager.h"
#include "EditorSceneSettings.h"
#include "EnvironmentSystem.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ComponentLights.h"
#include "ModuleCamera.h"
#include "FrustumDebugDraw.h"
#include "BoundingVolume.h"
#include "ComponentBounds.h"
#include "CollisionSystem.h"
#include "CollisionResponse.h"
#include "ShadowMath.h"
#include "Model.h"
#include "Mesh.h"
#include "MeshEntry.h"
#include "ResourceMesh.h"
#include <d3dx12.h>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <cfloat>

static constexpr float kDeg2Rad = 0.0174532925f;

RuntimeCore::RuntimeCore(bool standalone) : m_standalone(standalone){}
RuntimeCore::~RuntimeCore() = default;

bool RuntimeCore::init(){
    ModuleD3D12* d3d12 = app->getD3D12();
    ID3D12Device* device = d3d12->getDevice();

    ComPtr<ID3D12Device4> device4;
    device->QueryInterface(IID_PPV_ARGS(&device4));

    m_debugDraw = std::make_unique<DebugDrawPass>(device4.Get(), d3d12->getDrawCommandQueue(), false);
    m_collisionSystem = std::make_unique<CollisionSystem>();
    m_collisionResponse = std::make_unique<CollisionResponse>();
    m_sceneManager = std::make_unique<SceneManager>();
    m_meshRenderPass = std::make_unique<ForwardMeshPass>();
    m_hotReload = std::make_unique<HotReloadManager>();

    std::string scriptDir = app->getFileSystem()->GetAssetsPath() + std::string("Scripts/");
    app->getFileSystem()->CreateDir(scriptDir.c_str());
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

    m_shadowMapPass = std::make_unique<ShadowMapPass>();
    if (!m_shadowMapPass->init(device)) return false;

    m_deferredLightingPass = std::make_unique<DeferredLightingPass>();
    if (!m_deferredLightingPass->init(device)) return false;

    m_decalPass = std::make_unique<DecalPass>();
    if (!m_decalPass->init(device)){
        LOG("RuntimeCore: DecalPass init failed (non-fatal)");
        m_decalPass.reset();
    }

    m_billboardPass = std::make_unique<BillboardPass>();
    if (!m_billboardPass->init(device)){
        LOG("RuntimeCore: BillboardPass init failed (non-fatal)");
        m_billboardPass.reset();
    }

    m_trailPass = std::make_unique<TrailPass>();
    if (!m_trailPass->init(device)){
        LOG("RuntimeCore: TrailPass init failed (non-fatal)");
        m_trailPass.reset();
    }

    m_particlePass = std::make_unique<ParticlePass>();
    if (!m_particlePass->init(device)){
        LOG("RuntimeCore: ParticlePass init failed (non-fatal)");
        m_particlePass.reset();
    }

    m_tonemapPass = std::make_unique<TonemapPass>();
    if (!m_tonemapPass->init(device)) return false;

    m_bloomPass = std::make_unique<BloomPass>();
    if (!m_bloomPass->init(device)) return false;

    m_postProcessChain = std::make_unique<PostProcessChain>();
    if (!m_postProcessChain->init(device)) return false;

    m_colorLUT = std::make_unique<ColorLUT>();

    m_envSystem = std::make_unique<EnvironmentSystem>();
    if (!m_envSystem->init(device, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT, false)) return false;

    m_sceneManager->setScene(std::make_unique<EmptyScene>(), device);

    if (m_standalone){
        m_playerViewport = std::make_unique<EditorViewport>();
        m_playerViewport->rt = std::make_unique<RenderTexture>("PlayerColor", kSceneColorFormat, Vector4(0.05f, 0.05f, 0.1f, 1.0f), DXGI_FORMAT_D32_FLOAT, 1.0f);
        m_playerViewport->rtScratch = std::make_unique<RenderTexture>("PlayerColorScratch", kSceneColorFormat, Vector4(0.05f, 0.05f, 0.1f, 1.0f));
        m_playerViewport->display = std::make_unique<RenderTexture>("PlayerDisplay", DXGI_FORMAT_R8G8B8A8_UNORM, Vector4(0.05f, 0.05f, 0.1f, 1.0f));
        m_playerViewport->displayScratch = std::make_unique<RenderTexture>("PlayerDisplayScratch", DXGI_FORMAT_R8G8B8A8_UNORM, Vector4(0.05f, 0.05f, 0.1f, 1.0f));
        for (int i = 0; i < EditorViewport::kNumBloomMips; ++i)
            m_playerViewport->bloomMips[i] = std::make_unique<RenderTexture>("PlayerBloomMip", kSceneColorFormat, Vector4(0.f, 0.f, 0.f, 1.0f));

        const uint32_t w = d3d12->getWindowWidth();
        const uint32_t h = d3d12->getWindowHeight();
        m_playerViewport->rt->resize(w, h);
        m_playerViewport->rtScratch->resize(w, h);
        m_playerViewport->display->resize(w, h);
        m_playerViewport->displayScratch->resize(w, h);
        uint32_t mw = w, mh = h;
        for (int i = 0; i < EditorViewport::kNumBloomMips; ++i){
            mw = std::max(1u, mw / 2);
            mh = std::max(1u, mh / 2);
            m_playerViewport->bloomMips[i]->resize(mw, mh);
        }

        BuildSettings buildSettings;
        const std::string bsPath = app->getFileSystem()->GetLibraryPath() + "BuildSettings.json";
        if (buildSettings.Load(bsPath)){
            if (m_sceneManager->loadSceneByBuildIndex(0, buildSettings))
                applySkyboxFromSettings();
            else
                LOG("RuntimeCore: BuildSettings.json found but scene 0 failed to load");
        } else {
            LOG("RuntimeCore: No BuildSettings.json at '%s' — booting with an empty scene", bsPath.c_str());
        }
    }

    return true;
}

bool RuntimeCore::cleanUp(){
    m_envSystem.reset();
    m_debugDraw.reset();
    m_sceneManager.reset();
    m_gbufferPass.reset();
    m_deferredLightingPass.reset();
    m_decalPass.reset();
    m_tonemapPass.reset();
    m_bloomPass.reset();
    m_postProcessChain.reset();
    m_colorLUT.reset();
    if (m_skinningPass){ m_skinningPass->cleanUp(); m_skinningPass.reset(); }
    if (m_hotReload) m_hotReload->unloadAll();
    return true;
}

SceneGraph* RuntimeCore::getActiveModuleScene() const{
    return m_sceneManager ? m_sceneManager->getModuleScene() : nullptr;
}

void RuntimeCore::applySkyboxFromSettings(){
    if (!m_sceneManager || !m_envSystem) return;
    const EditorSceneSettings::Skybox& sky = m_sceneManager->getSettings().skybox;
    if (!sky.enabled || sky.cubemapPath.empty()) return;

    std::string ext = std::filesystem::path(sky.cubemapPath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".hdr") m_envSystem->loadHDR(sky.cubemapPath);
    else m_envSystem->load(sky.cubemapPath);
}

void RuntimeCore::tick(float dt, float aspectRatio){
    if (m_sceneManager){
        m_sceneManager->update(dt);
        m_sceneManager->updateAnimations(dt);
    }

    if (ModuleCamera* cam = app->getCamera()){
        if (aspectRatio > 0.f) cam->aspectRatio = aspectRatio;

        SceneGraph* scene = getActiveModuleScene();
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
                        // Octree query is a conservative broad phase (tests node regions,
                        // not entries). Confirm each candidate with an exact AABB test.
                        bool vis = visibleLookup.count(e.go) != 0 &&
                                   cam->getGameFrustum().intersectsAABB(e.worldAABB.min, e.worldAABB.max);
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

    SceneGraph* activeScene = getActiveModuleScene();
    if (m_collisionSystem)
        m_collisionSystem->run(activeScene, dt);

    const bool isPlaying = m_sceneManager &&
        m_sceneManager->getState() == SceneManager::PlayState::Playing;
    if (isPlaying && m_collisionResponse && m_collisionSystem)
        m_collisionResponse->solve(m_collisionSystem->getResults().contacts, dt);

    // Reset per-frame ring-buffer cursors before this frame's scene render(s).
    if (m_shadowMapPass) m_shadowMapPass->beginFrame();
    if (m_billboardPass) m_billboardPass->beginFrame();
    if (m_trailPass) m_trailPass->beginFrame();
    if (m_particlePass) m_particlePass->beginFrame();
}

void RuntimeCore::preRender(){
    if (!m_standalone) return;
    ModuleD3D12* d3d12 = app->getD3D12();

    const uint32_t curW = d3d12->getWindowWidth();
    const uint32_t curH = d3d12->getWindowHeight();
    if (m_playerViewport && m_playerViewport->rt &&
        (m_playerViewport->rt->getWidth() != curW || m_playerViewport->rt->getHeight() != curH) &&
        curW > 0 && curH > 0){
        d3d12->flush();
        m_playerViewport->rt->resize(curW, curH);
        m_playerViewport->rtScratch->resize(curW, curH);
        m_playerViewport->display->resize(curW, curH);
        m_playerViewport->displayScratch->resize(curW, curH);
        uint32_t mw = curW, mh = curH;
        for (int i = 0; i < EditorViewport::kNumBloomMips; ++i){
            mw = std::max(1u, mw / 2);
            mh = std::max(1u, mh / 2);
            m_playerViewport->bloomMips[i]->resize(mw, mh);
        }
    }

    const float dt = static_cast<float>(app->getElapsedMilis()) * 0.001f;
    const float aspect = (curH > 0) ? float(curW) / float(curH) : 0.f;
    tick(dt, aspect);
}

void RuntimeCore::render(){
    if (!m_standalone) return;
    renderStandaloneFrame();
}

void RuntimeCore::renderStandaloneFrame(){
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleShaderDescriptors* descs = app->getShaderDescriptors();
    ID3D12GraphicsCommandList* cmd = d3d12->getCommandList();

    const uint32_t w = m_playerViewport && m_playerViewport->rt ? m_playerViewport->rt->getWidth() : 0;
    const uint32_t h = m_playerViewport && m_playerViewport->rt ? m_playerViewport->rt->getHeight() : 0;
    if (w == 0 || h == 0) return;

    GameObject* activeCamGO = app->getCamera() ? app->getCamera()->getActiveCamera() : nullptr;
    ComponentCamera* cam = activeCamGO ? activeCamGO->getComponent<ComponentCamera>() : nullptr;
    ComponentTransform* camT = activeCamGO ? activeCamGO->getTransform() : nullptr;
    if (!cam || !camT) return;

    Matrix world = camT->getGlobalMatrix();
    Vector3 pos = world.Translation();
    Vector3 fwd = Vector3::TransformNormal(-Vector3::UnitZ, world); fwd.Normalize();
    Vector3 up = Vector3::TransformNormal(Vector3::UnitY, world); up.Normalize();
    Matrix view = Matrix::CreateLookAt(pos, pos + fwd, up);
    Matrix proj = Matrix::CreatePerspectiveFieldOfView(cam->getFOV(), float(w) / float(h), cam->getNearPlane(), cam->getFarPlane());

    cmd->Reset(d3d12->getCommandAllocator(), nullptr);
    ID3D12DescriptorHeap* heaps[] = { descs->getHeap(), app->getSamplerHeap()->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    ModuleStaticBuffer* sb = app->getStaticBuffer();
    if (sb && sb->isInitialized()){
        app->getResources()->uploadPendingMeshes(cmd, sb);
        sb->finalizeUploads(cmd);
    }

    m_playerViewport->rt->beginRender(cmd);
    renderSceneWithCamera(cmd, view, proj, w, h, /*editorExtras=*/false, m_playerViewport->rt.get());
    m_playerViewport->rt->endRender(cmd);

    PostProcessChain* chain = m_postProcessChain.get();
    RenderTexture* hdrResult = m_playerViewport->rt.get();
    if (chain)
        hdrResult = chain->run(cmd, PostProcessEffectDef::Domain::PreTonemap, m_playerViewport->rt.get(), m_playerViewport->rtScratch.get());

    const EditorSceneSettings* settings = m_sceneManager ? &m_sceneManager->getSettings() : nullptr;

    RenderTexture* bloomResult = nullptr;
    if (m_bloomPass && settings && settings->postProcess.bloomEnabled){
        m_bloomPass->render(cmd, hdrResult, *m_playerViewport, settings->postProcess.bloomThreshold);
        bloomResult = m_playerViewport->bloomMips[0].get();
    }

    const int nPostGamma = chain ? chain->countEnabled(PostProcessEffectDef::Domain::PostGamma) : 0;
    RenderTexture* tonemapTarget = (nPostGamma % 2 == 0) ? m_playerViewport->display.get() : m_playerViewport->displayScratch.get();
    RenderTexture* tonemapOther = (nPostGamma % 2 == 0) ? m_playerViewport->displayScratch.get() : m_playerViewport->display.get();

    TonemapParams tp;
    if (settings){
        tp.exposure = settings->postProcess.exposure;
        tp.bloomIntensity = settings->postProcess.bloomIntensity;
        tp.lutEnabled = settings->postProcess.lutEnabled;
    }
    if (m_tonemapPass) m_tonemapPass->render(cmd, hdrResult, tonemapTarget, bloomResult, m_colorLUT.get(), tp);
    if (chain && nPostGamma > 0)
        chain->run(cmd, PostProcessEffectDef::Domain::PostGamma, tonemapTarget, tonemapOther);

    auto toCopySrc = CD3DX12_RESOURCE_BARRIER::Transition(tonemapTarget->getTexture(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
    auto toCopyDst = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->getBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_RESOURCE_BARRIER preCopy[] = { toCopySrc, toCopyDst };
    cmd->ResourceBarrier(2, preCopy);
    cmd->CopyResource(d3d12->getBackBuffer(), tonemapTarget->getTexture());
    auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(tonemapTarget->getTexture(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->getBackBuffer(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    D3D12_RESOURCE_BARRIER postCopy[] = { toSRV, toPresent };
    cmd->ResourceBarrier(2, postCopy);

    cmd->Close();
    ID3D12CommandList* lists[] = { cmd };
    d3d12->getDrawCommandQueue()->ExecuteCommandLists(1, lists);
}

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

void RuntimeCore::renderSceneWithCamera(ID3D12GraphicsCommandList* cmd, const Matrix& view, const Matrix& proj, uint32_t w, uint32_t h, bool editorExtras, RenderTexture* outputRT){
    ModuleCamera* camera = app->getCamera();
    SceneGraph* moduleScene = getActiveModuleScene();

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

        for (auto& e : ownedEntries){
            Mesh* m = e.meshRes ? e.meshRes->getMesh() : e.mesh;
            if (e.isSkinned || !m || !m->hasAABB()){ e.hasWorldAABB = false; continue; }
            Matrix wm; memcpy(&wm, e.worldMatrix, sizeof(float) * 16);
            const Vector3 lmn = m->getAABBMin();
            const Vector3 lmx = m->getAABBMax();
            Vector3 mn(FLT_MAX, FLT_MAX, FLT_MAX);
            Vector3 mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            for (int c = 0; c < 8; ++c){
                Vector3 corner((c & 1) ? lmx.x : lmn.x,
                               (c & 2) ? lmx.y : lmn.y,
                               (c & 4) ? lmx.z : lmn.z);
                Vector3 wc = Vector3::Transform(corner, wm);
                mn = Vector3::Min(mn, wc);
                mx = Vector3::Max(mx, wc);
            }
            e.aabbMin = mn; e.aabbMax = mx; e.hasWorldAABB = true;
        }

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

    ShadowRenderData shadowData;
    if (m_shadowMapPass && !opaqueMeshes.empty()){
        ComponentDirectionalLight* caster = nullptr;
        if (moduleScene){
            std::function<void(GameObject*)> findCaster = [&](GameObject* n){
                if (!n || !n->isActive() || caster) return;
                if (auto* dl = n->getComponent<ComponentDirectionalLight>(); dl && dl->enabled){
                    caster = dl; return;
                }
                for (auto* c : n->getChildren()) findCaster(c);
            };
            findCaster(moduleScene->getRoot());
        }

        if (caster && caster->castShadows){
            float camNear, camFar;
            ShadowMath::ExtractNearFar(proj, camNear, camFar);
            const float farLimit = std::min(camFar, caster->shadowDistance);
            const uint32_t res = (uint32_t)caster->shadowResolution;

            bool didGpu = false;
            if (caster->shadowGpuFrustum && m_gbufferPass){
                Matrix ivp; (view * proj).Invert(ivp);
                Vector3 ld = caster->direction; ld.Normalize();
                if (m_shadowMapPass->computeGpuLightMatrix(cmd, m_gbufferPass->getGBuffer(),
                                                           ivp, ld, caster->shadowSunDistance)){
                    m_shadowMapPass->renderDirectionalGpu(cmd, opaqueMeshes, res);
                    shadowData.enabled = true;
                    shadowData.cascadeCount = 1;
                    shadowData.gpuMode = true;
                    shadowData.gpuVpVA = m_shadowMapPass->getGpuVpVA();
                    shadowData.lightDir = ld;
                    shadowData.bias = caster->shadowBias;
                    shadowData.pcfRadius = caster->shadowPcfRadius;
                    shadowData.mode = 0;
                    shadowData.ambientStrength = caster->shadowAmbientStrength;
                    shadowData.resolution = m_shadowMapPass->getResolution();
                    shadowData.srv = m_shadowMapPass->getSrvHandle();
                    didGpu = true;
                }
            }

            const int count = std::max(1, std::min(caster->shadowCascadeCount,
                                                   ShadowMath::kMaxCascades));
            if (!didGpu){

            float rangeNear = camNear;
            float rangeFar = farLimit;
            if (caster->shadowTightFrustum){
                float minDistance = FLT_MAX;
                float maxDistance = -FLT_MAX;
                for (MeshEntry* entry : opaqueMeshes){
                    if (!entry) continue;
                    Mesh* mesh = entry->meshRes ? entry->meshRes->getMesh() : entry->mesh;
                    if (!mesh || !mesh->hasAABB()) continue;
                    const Vector3 mn = mesh->getAABBMin();
                    const Vector3 mx = mesh->getAABBMax();
                    Matrix world;
                    memcpy(&world, entry->worldMatrix, sizeof(float) * 16);
                    const Vector3 localCenter = (mn + mx) * 0.5f;
                    const float localRadius = ((mx - mn) * 0.5f).Length();
                    const float sx = Vector3(world._11, world._12, world._13).Length();
                    const float sy = Vector3(world._21, world._22, world._23).Length();
                    const float sz = Vector3(world._31, world._32, world._33).Length();
                    const float radius = localRadius * std::max(sx, std::max(sy, sz));
                    const Vector3 worldCenter = Vector3::Transform(localCenter, world);
                    const Vector3 viewCenter = Vector3::Transform(worldCenter, view);
                    minDistance = std::min(minDistance, -viewCenter.z - radius);
                    maxDistance = std::max(maxDistance, -viewCenter.z + radius);
                }
                if (minDistance < maxDistance){
                    rangeNear = std::max(rangeNear, minDistance);
                    rangeFar = std::min(rangeFar, maxDistance);
                    if (rangeFar <= rangeNear + 0.01f){ rangeNear = camNear; rangeFar = farLimit; }
                }
            }

            float splits[ShadowMath::kMaxCascades];
            ShadowMath::CascadeSplits(rangeNear, rangeFar, count,
                                      caster->shadowCascadeLambda, splits);

            static const float kCascadeColors[4][3] = {
                { 1.f, 0.4f, 0.4f }, { 0.4f, 1.f, 0.4f },
                { 0.4f, 0.5f, 1.f }, { 1.f, 1.f, 0.4f },
            };

            Matrix cascadeVP[ShadowMath::kMaxCascades];
            float prevFar = rangeNear;
            for (int c = 0; c < count; ++c){
                ShadowMath::DirShadowResult sm = ShadowMath::DirectionalLightViewProj(
                    view, proj, caster->direction, prevFar, splits[c],
                    caster->shadowSunDistance, res);
                cascadeVP[c] = sm.viewProj;
                shadowData.cascadeSplit[c] = splits[c];
                prevFar = splits[c];

                if (editorExtras && caster->shadowDebugCascades)
                    dd::sphere(ddConvert(sm.center), kCascadeColors[c % 4], sm.radius);
            }

            m_shadowMapPass->render(cmd, opaqueMeshes, cascadeVP, count, res,
                                    caster->shadowMode, caster->shadowExpK,
                                    caster->shadowLightBleed,
                                    caster->shadowStaggerCascades);

            for (int c = 0; c < count; ++c)
                shadowData.lightViewProj[c] = cascadeVP[c];

            shadowData.enabled = true;
            shadowData.cascadeCount = count;
            shadowData.lightDir = caster->direction;
            shadowData.lightDir.Normalize();
            shadowData.bias = caster->shadowBias;
            shadowData.pcfRadius = caster->shadowPcfRadius;
            shadowData.mode = caster->shadowMode;
            shadowData.expK = caster->shadowExpK;
            shadowData.lightBleed = caster->shadowLightBleed;
            shadowData.ambientStrength = caster->shadowAmbientStrength;
            shadowData.debugTint = caster->shadowDebugCascades;
            shadowData.resolution = m_shadowMapPass->getResolution();
            shadowData.srv = m_shadowMapPass->getSrvHandle();
            if (m_shadowMapPass->hasMoments())
                shadowData.momentSrv = m_shadowMapPass->getMomentsSrvHandle();
            }

            if (editorExtras && caster->shadowShowPreview)
                m_shadowMapPass->copyPreview(cmd, caster->shadowPreviewCascade);
        }

        {
            ComponentSpotLight* spot = nullptr; GameObject* spotGO = nullptr;
            std::function<void(GameObject*)> find = [&](GameObject* n){
                if (!n || !n->isActive() || spot) return;
                if (auto* sl = n->getComponent<ComponentSpotLight>(); sl && sl->enabled && sl->castShadows){
                    spot = sl; spotGO = n; return;
                }
                for (auto* c : n->getChildren()) find(c);
            };
            if (moduleScene) find(moduleScene->getRoot());
            if (spot && spotGO){
                Vector3 pos = spotGO->getTransform()->getGlobalMatrix().Translation();
                Matrix vp = ShadowMath::SpotLightViewProj(pos, spot->direction,
                                spot->outerAngle * 3.14159265f / 180.f, spot->radius);
                m_shadowMapPass->renderSpot(cmd, opaqueMeshes, vp, (uint32_t)spot->shadowResolution);
                shadowData.spotEnabled = true;
                shadowData.spotViewProj = vp;
                shadowData.spotPos = pos;
                shadowData.spotBias = spot->shadowBias;
                shadowData.spotPcfRadius = spot->shadowPcfRadius;
                shadowData.spotResolution = m_shadowMapPass->getSpotResolution();
                shadowData.spotSrv = m_shadowMapPass->getSpotSrvHandle();
            }
        }

        {
            ComponentPointLight* pt = nullptr; GameObject* ptGO = nullptr;
            std::function<void(GameObject*)> find = [&](GameObject* n){
                if (!n || !n->isActive() || pt) return;
                if (auto* pl = n->getComponent<ComponentPointLight>(); pl && pl->enabled && pl->castShadows){
                    pt = pl; ptGO = n; return;
                }
                for (auto* c : n->getChildren()) find(c);
            };
            if (moduleScene) find(moduleScene->getRoot());
            if (pt && ptGO){
                Vector3 pos = ptGO->getTransform()->getGlobalMatrix().Translation();
                Matrix faces[6];
                ShadowMath::PointLightFaceViewProj(pos, 0.05f, pt->radius, faces);
                m_shadowMapPass->renderPoint(cmd, opaqueMeshes, faces, pos, pt->radius,
                                             (uint32_t)pt->shadowResolution);
                shadowData.pointEnabled = true;
                shadowData.pointPos = pos;
                shadowData.pointRange = pt->radius;
                shadowData.pointBias = pt->shadowBias;
                shadowData.pointSrv = m_shadowMapPass->getPointSrvHandle();
            }
        }

        ID3D12DescriptorHeap* shHeaps[] = { app->getShaderDescriptors()->getHeap(),
                                            app->getSamplerHeap()->getHeap() };
        cmd->SetDescriptorHeaps(2, shHeaps);
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
                                                gbufferViewportIndex, shadowData);
            } else {
                m_deferredLightingPass->render(cmd, *m_gbufferPass, m_frameLights,
                                                viewCamPos, view, proj,
                                                invViewProj, envForIBL, w, h,
                                                gbufferViewportIndex, shadowData);
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
                                                 camPos, viewProj, envForIBL, shadowData);
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
                int colorIdx = 0;   // which palette color this part uses
            };
            std::vector<BoundsEntry> boundsEntries;

            std::function<void(GameObject*)> collectBounds = [&](GameObject* node){
                if (!node || !node->isActive()) return;
                if (auto* cm = node->getComponent<ComponentMesh>()){
                    BoundsEntry e;
                    bool hasEntry = false;
                    const ComponentBounds* cb = node->getComponent<ComponentBounds>();

                    if (cm->hasSkinData()){
                        const auto& joints = cm->getSkinJoints();
                        const int jn = (int)joints.size();
                        constexpr float kBig = FLT_MAX;

                        // Measure skeleton span and mesh span to decide strategy:
                        // A weapon/accessory mesh is much smaller than the full skeleton it
                        // references (GLTF exporters often put all bones in every skin).
                        // Detect this by comparing local AABB diagonal to joint span diagonal.
                        if (!cm->hasAABB()) cm->computeLocalAABB();
                        const bool localOK = cm->hasAABB();

                        Vector3 allJMin(kBig,kBig,kBig), allJMax(-kBig,-kBig,-kBig);
                        int rootJoint = -1;
                        for (int i = 0; i < jn; ++i){
                            if (!joints[i] || !joints[i]->getTransform()) continue;
                            Vector3 wp = joints[i]->getTransform()->getGlobalMatrix().Translation();
                            allJMin = Vector3::Min(allJMin, wp);
                            allJMax = Vector3::Max(allJMax, wp);
                        }
                        // Find root joint (first with no parent in joint list) for weapon transform
                        {
                            std::vector<bool> hasParent(jn, false);
                            for (int i = 0; i < jn; ++i){
                                if (!joints[i]) continue;
                                GameObject* p = joints[i]->getParent();
                                for (int j = 0; j < jn; ++j)
                                    if (i != j && joints[j] == p){ hasParent[i] = true; break; }
                            }
                            for (int i = 0; i < jn; ++i)
                                if (!hasParent[i] && joints[i]){ rootJoint = i; break; }
                        }

                        float jointSpan = (allJMax - allJMin).Length();
                        float meshSpan  = localOK ? (cm->getLocalAABBMax() - cm->getLocalAABBMin()).Length() : -1.f;

                        // Treat as weapon/accessory (one tight box from local AABB × root joint):
                        //  - mesh geometry is much smaller than the skeleton it references
                        //    (GLTF exporters sometimes bake all bones into every skin)
                        //  - OR joints are very tightly clustered (1-3 weapon/prop bones)
                        //  - OR very few joints total
                        const bool jointsAreTight = (jointSpan < 0.4f);
                        const bool fewJoints      = (jn <= 3);
                        const bool meshSmall      = localOK && meshSpan > 0.f && (meshSpan < jointSpan * 0.45f);
                        const bool isAccessory    = localOK && rootJoint >= 0 && (fewJoints || jointsAreTight || meshSmall);

                        if (isAccessory){
                            // Weapon/prop: use the proper skinning transform
                            // (joint_current_world × inverseBindMatrix) to map the
                            // rest-pose local AABB into world space correctly.
                            const auto& ibms = cm->getLocalSkin().inverseBindMatrices;
                            Matrix skinTransform = joints[rootJoint]->getTransform()->getGlobalMatrix();
                            if (rootJoint < (int)ibms.size())
                                skinTransform = ibms[rootJoint] * skinTransform;
                            Vector3 lMin = cm->getLocalAABBMin(), lMax = cm->getLocalAABBMax();
                            Vector3 corners[8] = {
                                {lMin.x,lMin.y,lMin.z},{lMax.x,lMin.y,lMin.z},
                                {lMin.x,lMax.y,lMin.z},{lMax.x,lMax.y,lMin.z},
                                {lMin.x,lMin.y,lMax.z},{lMax.x,lMin.y,lMax.z},
                                {lMin.x,lMax.y,lMax.z},{lMax.x,lMax.y,lMax.z},
                            };
                            Vector3 wMin(kBig,kBig,kBig), wMax(-kBig,-kBig,-kBig);
                            for (auto& c : corners){
                                Vector3 wc = Vector3::Transform(c, skinTransform);
                                wMin = Vector3::Min(wMin, wc); wMax = Vector3::Max(wMax, wc);
                            }
                            BoundsEntry be;
                            be.colorIdx = 0; // weapons/props -> palette slot 0
                            if (cb && cb->bvType == BVType::Sphere){
                                Vector3 center = (wMin + wMax) * 0.5f;
                                float radius = (cb->radiusOverride >= 0.f)
                                    ? cb->radiusOverride : (wMax - center).Length();
                                be.type = BVType::Sphere; be.sphere = { center, radius };
                            } else {
                                be.type = BVType::AABB; be.box = { wMin, wMax };
                            }
                            boundsEntries.push_back(be);
                        } else {
                            // Body/full-skeleton mesh: one box per major limb branch.
                            // Build parent + children maps within this joint list.
                            std::vector<int> jpar(jn, -1);
                            std::vector<std::vector<int>> jchildren(jn);
                            for (int i = 0; i < jn; ++i){
                                if (!joints[i]) continue;
                                GameObject* p = joints[i]->getParent();
                                for (int j = 0; j < jn; ++j)
                                    if (i != j && joints[j] == p){ jpar[i] = j; jchildren[j].push_back(i); break; }
                            }
                            std::function<void(int, std::vector<int>&)> collectDesc;
                            collectDesc = [&](int idx, std::vector<int>& out){
                                out.push_back(idx);
                                for (int c : jchildren[idx]) collectDesc(c, out);
                            };

                            // Single-pass split: walk each root down its chain to the first
                            // branch point, then collect each child sub-tree as its own group.
                            // This gives ~4-6 boxes (pelvis+spine, head+neck, L-arm, R-arm,
                            // L-leg, R-leg) without deep nesting.
                            std::vector<std::vector<int>> groups;
                            auto splitOnce = [&](int startIdx){
                                // Walk straight chain from startIdx to first branch/leaf
                                std::vector<int> chain;
                                int cur = startIdx;
                                while (true){
                                    chain.push_back(cur);
                                    if (jchildren[cur].size() == 1) cur = jchildren[cur][0];
                                    else break;
                                }
                                if (jchildren[cur].empty()){
                                    groups.push_back(chain); // leaf chain
                                } else {
                                    groups.push_back(chain); // connector (spine/pelvis)
                                    for (int c : jchildren[cur]){
                                        std::vector<int> g;
                                        collectDesc(c, g);
                                        groups.push_back(g);
                                    }
                                }
                            };

                            for (int i = 0; i < jn; ++i)
                                if (jpar[i] == -1 && joints[i]) splitOnce(i);

                            if (groups.empty()){
                                std::vector<int> g;
                                for (int i = 0; i < jn; ++i) if (joints[i]) g.push_back(i);
                                if (!g.empty()) groups.push_back(g);
                            }

                            constexpr float kPad = 0.12f;
                            int groupColor = 1; // body parts start at palette slot 1
                            for (const auto& group : groups){
                                Vector3 wMin(kBig,kBig,kBig), wMax(-kBig,-kBig,-kBig);
                                bool any = false;
                                for (int idx : group){
                                    if (!joints[idx] || !joints[idx]->getTransform()) continue;
                                    Vector3 wp = joints[idx]->getTransform()->getGlobalMatrix().Translation();
                                    wMin = Vector3::Min(wMin, wp); wMax = Vector3::Max(wMax, wp);
                                    any = true;
                                }
                                if (!any) continue;
                                const Vector3 pad(kPad, kPad, kPad);
                                wMin -= pad; wMax += pad;
                                BoundsEntry be;
                                be.colorIdx = groupColor++; // each limb a distinct color
                                if (cb && cb->bvType == BVType::Sphere){
                                    Vector3 center = (wMin + wMax) * 0.5f;
                                    float radius = (cb->radiusOverride >= 0.f)
                                        ? cb->radiusOverride : (wMax - center).Length();
                                    be.type = BVType::Sphere; be.sphere = { center, radius };
                                } else {
                                    be.type = BVType::AABB; be.box = { wMin, wMax };
                                }
                                boundsEntries.push_back(be);
                            }
                        }
                        hasEntry = false; // entries already pushed directly above
                    } else {
                        e.colorIdx = 3; // plain static meshes -> green
                        if (!cm->hasAABB()) cm->computeLocalAABB();
                        if (cm->hasAABB()){
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
                            hasEntry = true;
                        }
                    }
                    if (hasEntry) boundsEntries.push_back(e);
                }
                for (auto* child : node->getChildren()) collectBounds(child);
            };
            collectBounds(moduleScene->getRoot());

            // Distinct color palette so each body part / weapon is easy to tell apart.
            // Slot 0 is reserved for weapons/props; 1+ cycle through the limb colors.
            static const float kPartPalette[][3] = {
                { 1.00f, 0.20f, 0.90f }, // 0 weapon/prop   - magenta
                { 0.95f, 0.25f, 0.25f }, // 1 - red
                { 0.30f, 0.65f, 1.00f }, // 2 - blue
                { 0.30f, 0.95f, 0.40f }, // 3 - green
                { 1.00f, 0.80f, 0.15f }, // 4 - yellow
                { 0.20f, 0.95f, 0.95f }, // 5 - cyan
                { 1.00f, 0.55f, 0.10f }, // 6 - orange
                { 0.70f, 0.45f, 1.00f }, // 7 - purple
            };
            constexpr int kPaletteSize = (int)(sizeof(kPartPalette) / sizeof(kPartPalette[0]));

            const size_t N = boundsEntries.size();
            for (size_t i = 0; i < N; ++i){
                const BoundsEntry& e = boundsEntries[i];
                const float* color = kPartPalette[((e.colorIdx % kPaletteSize) + kPaletteSize) % kPaletteSize];
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

void RuntimeCore::gatherLights(GameObject* node, FrameLightData& out) const{
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

void RuntimeCore::gatherDecals(GameObject* node, std::vector<DecalInstance>& out,
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

void RuntimeCore::gatherBillboards(GameObject* node, std::vector<BillboardInstance>& out,
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

void RuntimeCore::gatherParticleSystems(GameObject* node, std::vector<BillboardInstance>& out,
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

void RuntimeCore::gatherTrails(GameObject* node, std::vector<TrailInstance>& out,
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

void RuntimeCore::gatherGPUParticles(GameObject* node,
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

void RuntimeCore::debugDrawLights(SceneGraph* scene, float sz){
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
