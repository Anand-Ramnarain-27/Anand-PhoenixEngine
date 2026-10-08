#pragma once
// The runtime: owns the scene, the simulation step and the whole render pipeline. Shared by the editor (which
// renders it into its viewports) and the standalone Player (which renders it to the window).

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
#include "FogPass.h"
#include "VolumetricFogPass.h"
#include "PostProcessChain.h"
#include "ColorLUT.h"
#include "XRayPass.h"
#include "EditorSceneSettings.h"

#include <memory>
#include <vector>
#include <string>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct ID3D12Device;
struct ID3D12GraphicsCommandList;

class SceneManager;
class SceneTransition;
class EnvironmentSystem;
class DebugDrawPass;
class CollisionSystem;
class CollisionResponse;
class NavigationSystem;
class RenderTexture;
class GameObject;
class SceneGraph;
class ModuleCamera;
class ComponentDirectionalLight;
struct EditorViewport;

/// Scene, simulation and rendering. `standalone` (the Player) drives its own frame: preRender() ticks and
/// render() draws to the back buffer. In the editor ModuleEditor calls tick() and renderSceneWithCamera() for
/// each viewport instead.
class RuntimeCore : public Module {
public:
    explicit RuntimeCore(bool standalone);
    ~RuntimeCore() override;

    bool init() override;
    bool cleanUp() override;
    void preRender() override;
    void render() override;

    /// One simulation step: scene and script update, level transitions, queued script requests, animation,
    /// culling against the game camera, collision and its response. `aspectRatio` <= 0 keeps the camera's.
    void tick(float dt, float aspectRatio);

    /// Records the whole scene pass (shadows, G-buffer, lighting, transparents, VFX, x-ray) into `outputRT`.
    /// `editorExtras` is true for the editor's Scene view: no game-camera culling, debug overlays (shadow cascades,
    /// debug draw), and the occlusion fade / x-ray only when the scene settings preview them there.
    void renderSceneWithCamera(ID3D12GraphicsCommandList* cmd, const Matrix& view, const Matrix& proj,
                                uint32_t w, uint32_t h, bool editorExtras, RenderTexture* outputRT = nullptr);

    SceneManager* getSceneManager() const { return m_sceneManager.get(); }
    ForwardMeshPass* getMeshRenderPass() const { return m_meshRenderPass.get(); }
    MeshPipeline* getMeshPipeline() const { return m_meshRenderPass ? &m_meshRenderPass->getPipeline() : nullptr; }
    EnvironmentSystem* getEnvSystem() const { return m_envSystem.get(); }
    DebugDrawPass* getDebugDraw() const { return m_debugDraw.get(); }
    CollisionSystem* getCollisionSystem() const { return m_collisionSystem.get(); }
    CollisionResponse* getCollisionResponse() const { return m_collisionResponse.get(); }
    NavigationSystem* getNavigationSystem() const { return m_navigationSystem.get(); }
    HotReloadManager* getHotReloadManager() const { return m_hotReload.get(); }

    GBufferPass* getGBufferPass() const { return m_gbufferPass.get(); }
    DeferredLightingPass* getDeferredLightingPass() const { return m_deferredLightingPass.get(); }
    ShadowMapPass* getShadowMapPass() const { return m_shadowMapPass.get(); }
    TonemapPass* getTonemapPass() const { return m_tonemapPass.get(); }
    BloomPass* getBloomPass() const { return m_bloomPass.get(); }
    FogPass* getFogPass() const { return m_fogPass.get(); }
    VolumetricFogPass* getVolumetricFogPass() const { return m_volumetricFogPass.get(); }
    PostProcessChain* getPostProcessChain() const { return m_postProcessChain.get(); }
    ColorLUT* getColorLUT() const { return m_colorLUT.get(); }

    const FrameLightData& getFrameLights() const { return m_frameLights; }
    const ShadowRenderData& getFrameShadowData() const { return m_frameShadowData; }

    /// Defined in RuntimeCoreCore.cpp so it links into GameScript.dll without the renderer.
    SceneGraph* getActiveModuleScene() const;
    int getFrameDrawCalls() const { return m_frameDrawCalls; }

    /// Loads the skybox the active scene's settings name (.hdr is converted to a cubemap).
    void applySkyboxFromSettings();

private:
    void installEngineHooks();
    void loadScriptLibraries();
    bool createRenderPasses(ID3D12Device* device);
    void createPlayerViewport(uint32_t w, uint32_t h);
    void resizePlayerViewport(uint32_t w, uint32_t h);
    void bootStandaloneScene();
    void cullScene(ModuleCamera& cam);
    void updatePlayerUI(uint32_t w, uint32_t h);

    // Steps of renderSceneWithCamera.
    struct SceneView;
    void gatherSceneMeshes(SceneView& v);
    void dispatchSkinning(ID3D12GraphicsCommandList* cmd, SceneView& v);
    void sortMeshesByPass(SceneView& v);
    void gatherEffects(SceneView& v);
    void renderShadows(ID3D12GraphicsCommandList* cmd, SceneView& v, ShadowRenderData& shadowData);
    void renderDirectionalShadows(ID3D12GraphicsCommandList* cmd, SceneView& v, const ComponentDirectionalLight& light,
                                  ShadowRenderData& shadowData);
    OcclusionParams buildOcclusionParams(const SceneView& v) const;
    void renderScenePasses(ID3D12GraphicsCommandList* cmd, SceneView& v, const ShadowRenderData& shadowData,
                           const OcclusionParams& occlusion);
    void drawEditorDebug(ID3D12GraphicsCommandList* cmd, SceneView& v);
    void drawBoundsDebug(SceneGraph* scene);

    bool m_standalone;

    std::unique_ptr<DebugDrawPass> m_debugDraw;
    std::unique_ptr<CollisionSystem> m_collisionSystem;
    std::unique_ptr<CollisionResponse> m_collisionResponse;
    std::unique_ptr<NavigationSystem> m_navigationSystem;
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
    std::unique_ptr<FogPass> m_fogPass;
    std::unique_ptr<VolumetricFogPass> m_volumetricFogPass;
    std::unique_ptr<PostProcessChain> m_postProcessChain;
    std::unique_ptr<ColorLUT> m_colorLUT;
    std::unique_ptr<EnvironmentSystem> m_envSystem;
    std::unique_ptr<HotReloadManager> m_hotReload;
    std::unique_ptr<SkinningPass> m_skinningPass;

    FrameLightData m_frameLights;
    ShadowRenderData m_frameShadowData;
    RenderOctree m_renderOctree;

    int m_frameDrawCalls = 0;

    std::unique_ptr<EditorViewport> m_playerViewport;

    /// Transient point lights (Phoenix::VFX, at most kMaxTransientLights) are gathered in a first pass so they always
    /// get a slot ahead of the level's lights; the light pass budgets levels to 28 of the 32 point slots.
    static constexpr int kMaxTransientLights = 4;
    void gatherLights(GameObject* node, FrameLightData& out, bool transientPass) const;
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
    RenderTexture* applyPlayerFog(ID3D12GraphicsCommandList* cmd, RenderTexture* hdrResult, const Matrix& view,
                                  const Matrix& proj, const Vector3& pos, const EditorSceneSettings::Fog& fog);

    std::unique_ptr<XRayPass> m_xrayPass;

    std::unique_ptr<SceneTransition> m_sceneTransition;
    bool m_clampNextDt = false;   // the next tick's delta includes a blocking transition load
    float m_vfxClock = 0.f;       // scaled game time, for VFX UV scroll
};
