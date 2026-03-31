#pragma once
#include "Module.h"
#include "EditorSelection.h"
#include "EditorPanels.h"
#include "MeshRenderPass.h"
#include "MeshPipeline.h"
#include "ShaderTableDesc.h"
#include "PrefabEditSession.h"
#include "HotReloadManager.h"
#include "ComponentScript.h"
#include "FileWatcher.h"
#include "SkinningPass.h"
#include "ComponentAnimation.h"

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
class EditorPanel;
class SceneViewPanel;
class GameViewPanel;
class GameObject;
class ComponentCamera;
class ComponentMesh;
class ModuleScene;
class FileDialog;

class ModuleEditor : public Module {
public:
    ModuleEditor();
    ~ModuleEditor();

    bool init() override;
    bool cleanUp() override;
    void preRender() override;
    void render() override;

    SceneManager* getSceneManager()    const { return m_sceneManager.get(); }
    MeshRenderPass* getMeshRenderPass()   const { return m_meshRenderPass.get(); }
    MeshPipeline* getMeshPipeline()     const { return m_meshRenderPass ? &m_meshRenderPass->getPipeline() : nullptr; }
    EnvironmentSystem* getEnvSystem()        const { return m_envSystem.get(); }
    DebugDrawPass* getDebugDraw()        const { return m_debugDraw.get(); }
    EditorSelection& getSelection() { return m_selection; }
    double             getGpuFrameTimeMs()   const { return m_gpuFrameTimeMs; }
    int                getSamplerType()      const { return m_samplerType; }
    void               setSamplerType(int t) { m_samplerType = t; }
    ModuleScene* getActiveModuleScene()const;
    ImVec2             getSceneViewSize()    const;

    void renderSceneWithCamera(ID3D12GraphicsCommandList* cmd, const Matrix& view, const Matrix& proj, uint32_t w, uint32_t h, bool editorExtras);

    void log(const char* text, const ImVec4& color = ImVec4(1, 1, 1, 1));
    GameObject* createEmptyGameObject(const char* name = "Empty", GameObject* parent = nullptr);
    void deleteGameObject(GameObject* go);
    void spawnAssetAtPath(const std::string& path);
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
    std::unique_ptr<ImGuiPass>       m_imguiPass;
    std::unique_ptr<DebugDrawPass>   m_debugDraw;
    std::unique_ptr<SceneManager>    m_sceneManager;
    std::unique_ptr<MeshRenderPass>  m_meshRenderPass;
    std::unique_ptr<EnvironmentSystem> m_envSystem;
    std::unique_ptr<HotReloadManager> m_hotReload;
    std::unique_ptr<SkinningPass> m_skinningPass;

    FileWatcher m_scriptWatcher;

    ShaderTableDesc m_descTable;

    ComPtr<ID3D12QueryHeap> m_gpuQueryHeap;
    ComPtr<ID3D12Resource>  m_gpuReadback;
    double m_gpuFrameTimeMs = 0.0;
    bool   m_gpuTimerReady = false;
    float  m_memoryUpdateTimer = 0.0f;

    std::vector<std::unique_ptr<EditorPanel>> m_ownedPanels;
    std::vector<EditorPanel*>                 m_panels;

    SceneViewPanel* m_sceneView = nullptr;
    GameViewPanel* m_gameView = nullptr;
    ConsolePanel* m_console = nullptr;
    PerformancePanel* m_performance = nullptr;

    template<typename T, typename... Args>
    T* addPanel(Args&&... args) {
        auto up = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = up.get();
        m_panels.push_back(raw);
        m_ownedPanels.push_back(std::move(up));
        return raw;
    }

    EditorSelection m_selection;
    FrameLightData  m_frameLights;
    int  m_samplerType = 0;
    bool m_firstFrame = true;

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
    void debugDrawLights(ModuleScene* scene, float lightSize);
    void updateMemory();
    void handleNewScenePopup(ID3D12GraphicsCommandList* cmd);
    void drawDockspace();
    void drawMenuBar();
    void handleDialogs();
    void flushExitPrefabEdit();
    void handleShortcuts();

    std::vector<ComPtr<ID3D12Resource>> m_frameTransientBuffers;
};