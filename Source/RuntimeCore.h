#pragma once
#include "Module.h"
#include "HotReloadManager.h"
#include "ForwardMeshPass.h"
#include "GBufferPass.h"
#include "DeferredLightingPass.h"
#include "DecalPass.h"
#include "BillboardPass.h"
#include "TrailPass.h"
#include "ParticlePass.h"
#include "SkinningPass.h"
#include "RenderOctree.h"
#include "TonemapPass.h"
#include "BloomPass.h"
#include "PostProcessChain.h"
#include "ColorLUT.h"

#include <memory>
#include <vector>
#include <string>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct ID3D12Device;
struct ID3D12GraphicsCommandList;

class SceneManager;
class EnvironmentSystem;
class DebugDrawPass;
class CollisionSystem;
class CollisionResponse;
class RenderTexture;
class GameObject;
class SceneGraph;
struct EditorViewport;

// Owns everything both the Editor and a standalone Player need to boot a
// scene, tick simulation, and render a frame — no ImGui/editor UI in here.
// The Editor drives tick()/renderSceneWithCamera() explicitly (see
// ModuleEditor); a standalone Player runs this as an ordinary Module and
// lets preRender()/render() do the whole frame into the swapchain.
class RuntimeCore : public Module {
public:
    explicit RuntimeCore(bool standalone);
    ~RuntimeCore() override;

    bool init() override;
    bool cleanUp() override;
    void preRender() override;
    void render() override;

    // Advances scene simulation, culling and physics for one frame.
    // aspectRatio <= 0 leaves the camera's current aspect ratio untouched.
    void tick(float dt, float aspectRatio);

    void renderSceneWithCamera(ID3D12GraphicsCommandList* cmd, const Matrix& view, const Matrix& proj,
                                uint32_t w, uint32_t h, bool editorExtras, RenderTexture* outputRT = nullptr);

    SceneManager* getSceneManager() const { return m_sceneManager.get(); }
    ForwardMeshPass* getMeshRenderPass() const { return m_meshRenderPass.get(); }
    MeshPipeline* getMeshPipeline() const { return m_meshRenderPass ? &m_meshRenderPass->getPipeline() : nullptr; }
    EnvironmentSystem* getEnvSystem() const { return m_envSystem.get(); }
    DebugDrawPass* getDebugDraw() const { return m_debugDraw.get(); }
    CollisionSystem* getCollisionSystem() const { return m_collisionSystem.get(); }
    CollisionResponse* getCollisionResponse() const { return m_collisionResponse.get(); }
    HotReloadManager* getHotReloadManager() const { return m_hotReload.get(); }

    GBufferPass* getGBufferPass() const { return m_gbufferPass.get(); }
    DeferredLightingPass* getDeferredLightingPass() const { return m_deferredLightingPass.get(); }
    ShadowMapPass* getShadowMapPass() const { return m_shadowMapPass.get(); }
    TonemapPass* getTonemapPass() const { return m_tonemapPass.get(); }
    BloomPass* getBloomPass() const { return m_bloomPass.get(); }
    PostProcessChain* getPostProcessChain() const { return m_postProcessChain.get(); }
    ColorLUT* getColorLUT() const { return m_colorLUT.get(); }

    SceneGraph* getActiveModuleScene() const;
    int getFrameDrawCalls() const { return m_frameDrawCalls; }

private:
    bool m_standalone;

    std::unique_ptr<DebugDrawPass> m_debugDraw;
    std::unique_ptr<CollisionSystem> m_collisionSystem;
    std::unique_ptr<CollisionResponse> m_collisionResponse;
    std::unique_ptr<SceneManager> m_sceneManager;
    std::unique_ptr<ForwardMeshPass> m_meshRenderPass;
    std::unique_ptr<GBufferPass> m_gbufferPass;
    std::unique_ptr<DeferredLightingPass> m_deferredLightingPass;
    std::unique_ptr<ShadowMapPass> m_shadowMapPass;
    std::unique_ptr<DecalPass> m_decalPass;
    std::unique_ptr<BillboardPass> m_billboardPass;
    std::unique_ptr<TrailPass> m_trailPass;
    std::unique_ptr<ParticlePass> m_particlePass;
    std::unique_ptr<TonemapPass> m_tonemapPass;
    std::unique_ptr<BloomPass> m_bloomPass;
    std::unique_ptr<PostProcessChain> m_postProcessChain;
    std::unique_ptr<ColorLUT> m_colorLUT;
    std::unique_ptr<EnvironmentSystem> m_envSystem;
    std::unique_ptr<HotReloadManager> m_hotReload;
    std::unique_ptr<SkinningPass> m_skinningPass;

    FrameLightData m_frameLights;
    RenderOctree m_renderOctree;

    int m_frameDrawCalls = 0;
    int m_frameMeshCount = 0;

    // Standalone (Player) only: the swapchain-sized target renderSceneWithCamera
    // draws into before the post-process chain resolves it to the backbuffer.
    // Held behind a pointer (defined type only needed in RuntimeCore.cpp) so
    // this header doesn't drag RenderTexture's full definition into every
    // translation unit that just wants RuntimeCore's scene/pass accessors.
    std::unique_ptr<EditorViewport> m_playerViewport;

    void gatherLights(GameObject* node, FrameLightData& out) const;
    void gatherDecals(GameObject* node, std::vector<DecalInstance>& out,
                      const Matrix& view, const Matrix& proj,
                      uint32_t w, uint32_t h) const;
    void gatherBillboards(GameObject* node, std::vector<BillboardInstance>& out,
                          const Matrix& view, const Matrix& viewProj,
                          const Vector3& camPos, const Vector3& camRight, const Vector3& camUp) const;
    void gatherParticleSystems(GameObject* node, std::vector<BillboardInstance>& out,
                               const Matrix& viewProj,
                               const Vector3& camPos, const Vector3& camRight, const Vector3& camUp) const;
    void gatherTrails(GameObject* node, std::vector<TrailInstance>& out,
                      const Matrix& viewProj, const Vector3& camPos) const;
    void gatherGPUParticles(GameObject* node, std::vector<ParticleDrawRequest>& out,
                            const Vector3& camPos, const Vector3& camRight, const Vector3& camUp,
                            float elapsedTime) const;
    void debugDrawLights(SceneGraph* scene, float lightSize);

    void renderStandaloneFrame();
};
