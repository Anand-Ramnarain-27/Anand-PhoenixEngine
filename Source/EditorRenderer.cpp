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

void ModuleEditor::preRender(){
    flushExitPrefabEdit();
    m_sceneView->handleResize();
    m_gameView->handleResize();
    m_imguiPass->startFrame();
    ImGuizmo::BeginFrame();
    handleShortcuts();
    const float dt = static_cast<float>(app->getElapsedMilis()) * 0.001f;

    // Keep the cull frustum's aspect ratio matched to the Game view so culling,
    // the debug frustum, and the actual Game render all agree.
    if (ModuleCamera* cam = app->getCamera()){
        const ImVec2 gv = m_gameView->viewport.size;
        if (gv.x > 0.f && gv.y > 0.f) cam->aspectRatio = gv.x / gv.y;
    }

    if (m_sceneManager){
        m_sceneManager->update(dt);
        m_sceneManager->updateAnimations(dt);
    }

    if (ModuleCamera* cam = app->getCamera()){
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

    if (m_effectsPlaying && m_sceneManager &&
        m_sceneManager->getState() != SceneManager::PlayState::Playing){
        updateEffectsInEditMode(dt);
    }

    SceneGraph* activeScene = getActiveModuleScene();
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
