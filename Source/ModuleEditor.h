#pragma once

#include "Module.h"
#include "SceneManager.h"
#include "ShaderTableDesc.h"
#include "MeshPipeline.h"
#include "FileDialog.h"
#include "EditorSceneSettings.h"
#include <memory>
#include <vector>
#include <imgui.h>
#include <ImGuizmo.h>

class ImGuiPass;
class RenderTexture;
class DebugDrawPass;
class GameObject;
class ComponentCamera;
class ComponentMesh;

class ModuleEditor : public Module
{
public:
    ModuleEditor();
    ~ModuleEditor();

    bool init() override;
    bool cleanUp() override;
    void preRender() override;
    void render() override;

    SceneManager* getSceneManager() const { return sceneManager.get(); }
    void log(const char* text, const ImVec4& color = ImVec4(1, 1, 1, 1));

private:
    std::unique_ptr<ImGuiPass>     imguiPass;
    std::unique_ptr<RenderTexture> viewportRT;
    std::unique_ptr<DebugDrawPass> debugDrawPass;
    std::unique_ptr<SceneManager>  sceneManager;
    std::unique_ptr<MeshPipeline>  meshPipeline;
    ShaderTableDesc                descTable;

    GameObject* selectedGameObject = nullptr;
    ImGuizmo::OPERATION   gizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE        gizmoMode = ImGuizmo::LOCAL;
    bool                  useSnap = false;
    float                 snapTranslate[3] = { 0.25f, 0.25f, 0.25f };
    float                 snapRotate = 15.0f;
    float                 snapScale = 0.1f;

    bool showHierarchy = true;
    bool showInspector = true;
    bool showConsole = true;
    bool showViewport = true;
    bool showPerformance = false;
    bool showAssetBrowser = true;
    bool showSceneSettings = true;

    bool firstFrame = true;

    ImVec2 viewportSize = { 0, 0 };
    ImVec2 viewportPos = { 0, 0 };
    ImVec2 lastViewportSize = { 0, 0 };
    bool   pendingViewportResize = false;
    UINT   pendingViewportWidth = 0;
    UINT   pendingViewportHeight = 0;

    struct ConsoleEntry { std::string text; ImVec4 color; };
    std::vector<ConsoleEntry> console;
    bool autoScrollConsole = true;

    static constexpr int FPS_HISTORY = 200;
    float fpsHistory[FPS_HISTORY] = {};
    int   fpsIndex = 0;

    ComPtr<ID3D12QueryHeap> gpuQueryHeap;
    ComPtr<ID3D12Resource>  gpuReadbackBuffer;
    double gpuFrameTimeMs = 0.0;
    bool   gpuTimerReady = false;

    uint64_t gpuMemoryMB = 0;
    uint64_t systemMemoryMB = 0;

    struct CameraConstants { Matrix viewProj; };
    struct ObjectConstants { Matrix world; };
    ComPtr<ID3D12Resource> cameraConstantBuffer;
    ComPtr<ID3D12Resource> objectConstantBuffer;

    FileDialog m_saveDialog;
    FileDialog m_loadDialog;
    bool showNewSceneConfirmation = false;

    bool        renamingObject = false;
    char        renameBuffer[256] = {};
    GameObject* renamingTarget = nullptr;

    void drawDockspace();
    void drawMenuBar();
    void drawGizmoToolbar();

    void drawHierarchy();
    void drawHierarchyNode(GameObject* go);
    void drawInspector();
    void drawConsole();
    void drawViewport();
    void drawViewportOverlay();
    void drawPerformanceWindow();
    void drawAssetBrowser();
    void drawSceneSettings();

    void drawGizmo();

    void hierarchyItemContextMenu(GameObject* go);
    void hierarchyBlankContextMenu();

    void drawComponentCamera(ComponentCamera* cam);
    void drawComponentMesh(ComponentMesh* mesh);

    void renderViewportToTexture(ID3D12GraphicsCommandList* cmd);
    void updateCameraConstants(const Matrix& view, const Matrix& proj);

    void updateFPS();
    void updateMemory();

    void        deleteGameObject(GameObject* go);
    GameObject* createEmptyGameObject(const char* name = "Empty",
        GameObject* parent = nullptr);

    static bool isChildOf(const GameObject* root, const GameObject* needle);
};