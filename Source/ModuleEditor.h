#pragma once
#include "Module.h"
#include "EditorSelection.h"
#include "PrimitiveFactory.h"
#include "EditorPanels.h"
#include "MeshRenderPass.h"
#include "MeshPipeline.h"
#include "ShaderTableDesc.h"
#include "PrefabEditSession.h"
#include "HotReloadManager.h"
#include "ComponentScript.h"
#include "FileWatcher.h"
#include "GBufferPass.h"
#include "DeferredLightingPass.h"
#include "DecalPass.h"
#include "BillboardPass.h"
#include "TrailPass.h"
#include "ParticlePass.h"
#include "SkinningPass.h"
#include "RenderOctree.h"

#include <memory>
#include <vector>
#include <string>
#include <deque>
#include <functional>
#include <imgui.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct EditorCommand {
    std::function<void()> execute;
    std::function<void()> undo;
};

struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;
struct ID3D12QueryHeap;

class SceneManager;
class EnvironmentSystem;
class ImGuiPass;
class DebugDrawPass;
class CollisionSystem;
class CollisionResponse;
class RenderTexture;
class EditorPanel;
class SceneViewPanel;
class GameViewPanel;
class AssetBrowserPanel;
class GameObject;
class ComponentCamera;
class ComponentMesh;
class ModuleScene;
class FileDialog;
class EngineDropTarget;

class ModuleEditor : public Module {
public:
    ModuleEditor();
    ~ModuleEditor();

    bool init() override;
    bool cleanUp() override;
    void preRender() override;
    void render() override;

    SceneManager* getSceneManager() const { return m_sceneManager.get(); }
    MeshRenderPass* getMeshRenderPass() const { return m_meshRenderPass.get(); }
    MeshPipeline* getMeshPipeline() const { return m_meshRenderPass ? &m_meshRenderPass->getPipeline() : nullptr; }
    EnvironmentSystem* getEnvSystem() const { return m_envSystem.get(); }
    DebugDrawPass* getDebugDraw() const { return m_debugDraw.get(); }
    CollisionSystem* getCollisionSystem() const { return m_collisionSystem.get(); }
    CollisionResponse* getCollisionResponse() const { return m_collisionResponse.get(); }
    EditorSelection& getSelection() { return m_selection; }
    double getGpuFrameTimeMs() const { return m_gpuFrameTimeMs; }
    bool isGpuTimerReady() const { return m_gpuTimerReady; }
    int getFrameDrawCalls() const { return m_frameDrawCalls; }
    int getSamplerType() const { return m_samplerType; }
    void setSamplerType(int t) { m_samplerType = t; }
    ModuleScene* getActiveModuleScene()const;
    ImVec2 getSceneViewSize() const;

    void renderSceneWithCamera(ID3D12GraphicsCommandList* cmd, const Matrix& view, const Matrix& proj, uint32_t w, uint32_t h, bool editorExtras, RenderTexture* outputRT = nullptr);

    GBufferPass* getGBufferPass() const { return m_gbufferPass.get(); }
    DeferredLightingPass* getDeferredLightingPass() const { return m_deferredLightingPass.get(); }

    void log(const char* text, const ImVec4& color = ImVec4(1, 1, 1, 1));
    GameObject* createEmptyGameObject(const char* name = "Empty", GameObject* parent = nullptr);
    void deleteGameObject(GameObject* go);
    void spawnAssetAtPath(const std::string& path);
    GameObject* spawnModel(const std::string& path);

    // Spawn a built-in primitive (generates geometry procedurally, no external file).
    // addPhysics = true attaches a Rigidbody so it falls under gravity immediately.
    GameObject* spawnPrimitive(PrimitiveType type,
                               const Vector3& position = Vector3::Zero,
                               const Vector3& scale = Vector3::One,
                               bool addPhysics = false);

    // Lecture 11 "Particle Systems I" — Exercise 1: builds the 4-emitter fire rig
    // (flames / brighter inner light / glow / sparks) as a parented GameObject group.
    GameObject* spawnFireParticleSystem(const Vector3& position = Vector3::Zero);
    GameObject* spawnSwordTrail(const Vector3& position = Vector3::Zero);
    // Trail + particles combined prefab — fire orb that leaves a comet trail when moved.
    GameObject* spawnFireComet(const Vector3& position = Vector3::Zero);

    // Stop playback, clear editor selection and undo stack so no stale pointers
    // remain after the scene is restored.
    void stopPlay();

    // Effects transport — preview particles + trails in edit mode without scene Play.
    // Called from ComponentTrail / ComponentParticleSystem onEditor() buttons.
    bool isEffectsPlaying() const { return m_effectsPlaying; }
    void effectsPlay()        { m_effectsPlaying = true;  m_effectsTime = 0.f; }
    void effectsStop();
    void effectsRestartAll();
    void effectsRestartSelected();

    static bool isChildOf(const GameObject* root, const GameObject* needle);

    void pushCommand(EditorCommand cmd);
    void undoToSavePoint();
    void redo();
    void copySelected();
    void pasteClipboard();
    void duplicateSelected();
    bool canUndo() const;
    bool canRedo() const;

    void enterPrefabEdit(const std::string& prefabName);
    void exitPrefabEdit();
    PrefabEditSession* getPrefabSession() { return &m_prefabSession; }

    HotReloadManager* getHotReloadManager() const { return m_hotReload.get(); }
    void onScriptFileEvent(const std::string& absPath, FileWatcher::Event ev);
    void notifyScriptComponentsReload(const std::string& dllPath);

private:
    std::unique_ptr<ImGuiPass> m_imguiPass;
    std::unique_ptr<DebugDrawPass> m_debugDraw;
    std::unique_ptr<CollisionSystem> m_collisionSystem;
    std::unique_ptr<CollisionResponse> m_collisionResponse;
    std::unique_ptr<SceneManager> m_sceneManager;
    std::unique_ptr<MeshRenderPass> m_meshRenderPass;
    std::unique_ptr<GBufferPass> m_gbufferPass;
    std::unique_ptr<DeferredLightingPass> m_deferredLightingPass;
    std::unique_ptr<DecalPass> m_decalPass;
    std::unique_ptr<BillboardPass> m_billboardPass;
    std::unique_ptr<TrailPass>     m_trailPass;
    std::unique_ptr<ParticlePass>  m_particlePass;
    std::unique_ptr<EnvironmentSystem> m_envSystem;
    std::unique_ptr<HotReloadManager> m_hotReload;
    std::unique_ptr<SkinningPass> m_skinningPass;

    FileWatcher m_scriptWatcher;

    ShaderTableDesc m_descTable;

    ComPtr<ID3D12QueryHeap> m_gpuQueryHeap;
    ComPtr<ID3D12Resource> m_gpuReadback;
    double m_gpuFrameTimeMs = 0.0;
    bool m_gpuTimerReady = false;
    float m_memoryUpdateTimer = 0.0f;

    // Live render stats updated each frame in renderSceneWithCamera().
    int m_frameDrawCalls = 0;
    int m_frameMeshCount = 0;

    std::vector<std::unique_ptr<EditorPanel>> m_ownedPanels;
    std::vector<EditorPanel*> m_panels;

    SceneViewPanel* m_sceneView = nullptr;
    GameViewPanel* m_gameView = nullptr;
    ConsolePanel* m_console = nullptr;
    PerformancePanel* m_performance = nullptr;
    AssetBrowserPanel* m_assetBrowser = nullptr;

    EngineDropTarget* m_dropTarget = nullptr;

    template<typename T, typename... Args>
    T* addPanel(Args&&... args){
        auto up = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = up.get();
        m_panels.push_back(raw);
        m_ownedPanels.push_back(std::move(up));
        return raw;
    }

    EditorSelection m_selection;
    FrameLightData m_frameLights;

    // Gap 1: hierarchical render (frustum) culling — built once per frame in
    // preRender() from all renderable GameObjects, queried against the active
    // game camera frustum when ModuleCamera::cullAlgorithm == Octree.
    RenderOctree m_renderOctree;
    int m_samplerType = 0;
    bool m_firstFrame = true;

    // Effects transport — controls particles/trails in editor (edit-mode preview).
    // Independent of scene Play state: lets you preview fx without pressing Play.
    bool  m_effectsPlaying = false;
    float m_effectsTime    = 0.f;

    static constexpr int kMaxUndoSteps = 200;
    std::deque<EditorCommand> m_undoStack;
    std::deque<EditorCommand> m_redoStack;
    int m_savePointIndex = 0;

    struct ClipboardEntry {
        std::string name;
        std::string serialized;
    };
    ClipboardEntry m_clipboard;

    std::unique_ptr<FileDialog> m_saveDialog;
    std::unique_ptr<FileDialog> m_loadDialog;
    std::string m_currentScenePath;
    bool m_showNewSceneConfirm = false;

    PrefabEditSession m_prefabSession;
    bool m_pendingExitPrefab = false;

    ComPtr<ID3D12Resource> createUploadBuffer(ID3D12Device*, SIZE_T, const wchar_t*);
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
    void debugDrawLights(ModuleScene* scene, float lightSize);
    void updateMemory();
    void updateEffectsInEditMode(float dt); // tick trails + particles when not in play mode
    void handleNewScenePopup(ID3D12GraphicsCommandList* cmd);
    void drawDockspace();
    void drawMenuBar();
    void drawStatusBar();
    void handleDialogs();
    void flushExitPrefabEdit();
    void handleShortcuts();
    void drawDragDropOverlay();

    std::vector<ComPtr<ID3D12Resource>> m_frameTransientBuffers;
};
