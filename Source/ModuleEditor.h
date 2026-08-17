#pragma once
#include "Module.h"
#include "EditorSelection.h"
#include "PrimitiveFactory.h"
#include "ConsolePanel.h"
#include "PerformancePanel.h"
#include "ResourcesPanel.h"
#include "CollisionDebugPanel.h"
#include "GPUMemoryPanel.h"
#include "ForwardMeshPass.h"
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
#include "TonemapPass.h"
#include "BloomPass.h"
#include "PostProcessChain.h"
#include "ColorLUT.h"

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
class SceneGraph;
class FileDialog;
class EngineDropTarget;
class ShadowMapPass;
class ForwardMeshPass;
class MeshPipeline;
class GBufferPass;
class DeferredLightingPass;
class TonemapPass;
class BloomPass;
class PostProcessChain;
class ColorLUT;
class HotReloadManager;

class ModuleEditor : public Module {
public:
    ModuleEditor();
    ~ModuleEditor();

    bool init() override;
    bool cleanUp() override;
    void preRender() override;
    void render() override;

    // Forwarded to Application::getRuntimeCore() — RuntimeCore is the single owner of
    // the scene/render state, shared with the standalone Player build.
    SceneManager* getSceneManager() const;
    ForwardMeshPass* getMeshRenderPass() const;
    MeshPipeline* getMeshPipeline() const;
    EnvironmentSystem* getEnvSystem() const;
    DebugDrawPass* getDebugDraw() const;
    CollisionSystem* getCollisionSystem() const;
    CollisionResponse* getCollisionResponse() const;
    EditorSelection& getSelection(){ return m_selection; }
    double getGpuFrameTimeMs() const { return m_gpuFrameTimeMs; }
    bool isGpuTimerReady() const { return m_gpuTimerReady; }
    int getFrameDrawCalls() const;
    int getSamplerType() const { return m_samplerType; }
    void setSamplerType(int t){ m_samplerType = t; }
    SceneGraph* getActiveModuleScene()const;
    ImVec2 getSceneViewSize() const;

    void renderSceneWithCamera(ID3D12GraphicsCommandList* cmd, const Matrix& view, const Matrix& proj, uint32_t w, uint32_t h, bool editorExtras, RenderTexture* outputRT = nullptr);

    GBufferPass* getGBufferPass() const;
    DeferredLightingPass* getDeferredLightingPass() const;
    ShadowMapPass* getShadowMapPass() const;
    TonemapPass* getTonemapPass() const;
    BloomPass* getBloomPass() const;
    PostProcessChain* getPostProcessChain() const;
    ColorLUT* getColorLUT() const;

    void log(const char* text, const ImVec4& color = ImVec4(1, 1, 1, 1));
    GameObject* createEmptyGameObject(const char* name = "Empty", GameObject* parent = nullptr);
    void setupDefaultScene();
    void applySkyboxFromSettings();
    void deleteGameObject(GameObject* go);
    void spawnAssetAtPath(const std::string& path);
    GameObject* spawnModel(const std::string& path);

    GameObject* spawnPrimitive(PrimitiveType type,
                               const Vector3& position = Vector3::Zero,
                               const Vector3& scale = Vector3::One,
                               bool addPhysics = false);

    GameObject* spawnFireParticleSystem(const Vector3& position = Vector3::Zero);
    GameObject* spawnSwordTrail(const Vector3& position = Vector3::Zero);
    GameObject* spawnFireComet(const Vector3& position = Vector3::Zero);

    void stopPlay();

    bool isEffectsPlaying() const { return m_effectsPlaying; }
    void effectsPlay(){ m_effectsPlaying = true; m_effectsTime = 0.f; }
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
    PrefabEditSession* getPrefabSession(){ return &m_prefabSession; }

    HotReloadManager* getHotReloadManager() const;
    void onScriptFileEvent(const std::string& absPath, FileWatcher::Event ev);
    void notifyScriptComponentsReload(const std::string& dllPath);

private:
    std::unique_ptr<ImGuiPass> m_imguiPass;

    FileWatcher m_scriptWatcher;

    ShaderTableDesc m_descTable;

    ComPtr<ID3D12QueryHeap> m_gpuQueryHeap;
    ComPtr<ID3D12Resource> m_gpuReadback;
    double m_gpuFrameTimeMs = 0.0;
    bool m_gpuTimerReady = false;
    float m_memoryUpdateTimer = 0.0f;

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
    int m_samplerType = 0;
    bool m_firstFrame = true;

    bool m_effectsPlaying = false;
    float m_effectsTime = 0.f;

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
    void updateMemory();
    void updateEffectsInEditMode(float dt);
    void handleNewScenePopup(ID3D12GraphicsCommandList* cmd);
    void drawDockspace();
    void drawMenuBar();
    void drawStatusBar();
    void drawShadowMapPreview();
    void handleDialogs();
    void flushExitPrefabEdit();
    void handleShortcuts();
    void drawDragDropOverlay();

    std::vector<ComPtr<ID3D12Resource>> m_frameTransientBuffers;
};
