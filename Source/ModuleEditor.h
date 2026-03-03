#pragma once
#include "Module.h"
#include "SceneManager.h"
#include "ShaderTableDesc.h"
#include "MeshPipeline.h"
#include "FileDialog.h"
#include "EditorSceneSettings.h"
#include "EnvironmentSystem.h"
#include "EditorSelection.h"
#include <memory>
#include <vector>
#include <imgui.h>

class ImGuiPass;
class DebugDrawPass;
class EditorPanel;
class SceneViewPanel;
class GameViewPanel;
class HierarchyPanel;
class InspectorPanel;
class ConsolePanel;
class PerformancePanel;
class AssetBrowserPanel;
class SceneSettingsPanel;
class GameObject;
class ComponentCamera;
class ComponentMesh;
class ModuleScene;

class ModuleEditor : public Module
{
public:
    ModuleEditor();
    ~ModuleEditor();

    bool init()     override;
    bool cleanUp()  override;
    void preRender() override;
    void render()   override;

    SceneManager* getSceneManager()     const { return m_sceneManager.get(); }
    MeshPipeline* getMeshPipeline()     const { return m_meshPipeline.get(); }
    EnvironmentSystem* getEnvSystem()      const { return m_envSystem.get(); }
    DebugDrawPass* getDebugDraw()        const { return m_debugDraw.get(); }
    EditorSelection& getSelection() { return m_selection; }
    double getGpuFrameTimeMs()   const { return m_gpuFrameTimeMs; }
    int getSamplerType()      const { return m_samplerType; }
    void setSamplerType(int t) { m_samplerType = t; }
    ModuleScene* getActiveModuleScene() const;

    void renderSceneWithCamera(
        ID3D12GraphicsCommandList* cmd,
        const Matrix& view, const Matrix& proj,
        uint32_t w, uint32_t h,
        bool editorExtras);  

    void log(const char* text, const ImVec4& color = ImVec4(1, 1, 1, 1));
    GameObject* createEmptyGameObject(const char* name = "Empty", GameObject* parent = nullptr);
    void        deleteGameObject(GameObject* go);
    static bool isChildOf(const GameObject* root, const GameObject* needle);

    void drawComponentCamera(ComponentCamera* cam);
    void drawComponentMesh(ComponentMesh* mesh);

    ImVec2  getSceneViewSize() const;

private:
    std::unique_ptr<ImGuiPass>         m_imguiPass;
    std::unique_ptr<DebugDrawPass>     m_debugDraw;
    std::unique_ptr<SceneManager>      m_sceneManager;
    std::unique_ptr<MeshPipeline>      m_meshPipeline;
    std::unique_ptr<EnvironmentSystem> m_envSystem;
    ShaderTableDesc                    m_descTable;

    ComPtr<ID3D12QueryHeap> m_gpuQueryHeap;
    ComPtr<ID3D12Resource>  m_gpuReadback;
    double                  m_gpuFrameTimeMs = 0.0;
    bool                    m_gpuTimerReady = false;
    float m_memoryUpdateTimer = 0.0f;

    ComPtr<ID3D12Resource>  m_cameraCB;
    ComPtr<ID3D12Resource>  m_objectCB;
    ComPtr<ID3D12Resource>  m_lightCB;

    std::unique_ptr<SceneViewPanel>    m_sceneView;
    std::unique_ptr<GameViewPanel>     m_gameView;
    std::unique_ptr<HierarchyPanel>    m_hierarchy;
    std::unique_ptr<InspectorPanel>    m_inspector;
    std::unique_ptr<ConsolePanel>      m_console;
    std::unique_ptr<PerformancePanel>  m_performance;
    std::unique_ptr<AssetBrowserPanel> m_assetBrowser;
    std::unique_ptr<SceneSettingsPanel> m_sceneSettings;

    EditorSelection m_selection;
    int             m_samplerType = 0;
    bool            m_firstFrame = true;

    FileDialog  m_saveDialog;
    FileDialog  m_loadDialog;
    std::string m_currentScenePath;
    bool        m_showNewSceneConfirm = false;

    struct CameraConstants { Matrix viewProj; };
    struct ObjectConstants { Matrix world; };

    ComPtr<ID3D12Resource> createUploadBuffer(ID3D12Device* device, SIZE_T size, const wchar_t* name);
    void gatherLights(GameObject* node, MeshPipeline::LightCB& out) const;
    void debugDrawLights(ModuleScene* scene, float lightSize);
    void updateMemory();
    void handleNewScenePopup(ID3D12GraphicsCommandList* cmd);
    void drawDockspace();
    void drawMenuBar();
    void handleDialogs();
};