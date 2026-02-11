#pragma once

#include "Module.h"
#include "SceneManager.h"
#include "ShaderTableDesc.h"

#include <memory>
#include <vector>
#include <functional>
#include <imgui.h>

class ImGuiPass;
class RenderTexture;
class DebugDrawPass;
class GameObject;
class IScene;
class ModuleCamera;

struct SceneEntry
{
    const char* name;
    std::function<std::unique_ptr<IScene>()> create;
};

class ModuleEditor : public Module
{
public:
    ModuleEditor();
    ~ModuleEditor();

    bool init() override;
    bool cleanUp() override;
    void preRender() override;
    void render() override;

private:
    // Core systems
    std::unique_ptr<ImGuiPass> imguiPass;
    std::unique_ptr<RenderTexture> viewportRT;
    std::unique_ptr<DebugDrawPass> debugDrawPass;
    std::unique_ptr<SceneManager> sceneManager;

    ShaderTableDesc descTable;

    // Scene selection
    std::vector<SceneEntry> availableScenes;
    int selectedSceneIndex = -1;

    // Selection
    GameObject* selectedGameObject = nullptr;

    // Window toggles
    bool showHierarchy = true;
    bool showInspector = true;
    bool showConsole = true;
    bool showViewport = true;
    bool showPerformance = false;
    bool showExercises = true;
    bool showEditor = true;

    // Docking
    bool firstFrame = true;

    // Viewport
    ImVec2 viewportSize = { 0,0 };
    ImVec2 viewportPos = { 0,0 };
    ImVec2 lastViewportSize = { 0,0 };
    bool pendingViewportResize = false;
    UINT pendingViewportWidth = 0;
    UINT pendingViewportHeight = 0;

    // Console
    struct ConsoleEntry
    {
        std::string text;
        ImVec4 color;
    };
    std::vector<ConsoleEntry> console;
    bool autoScrollConsole = true;

    // FPS
    static constexpr int FPS_HISTORY = 200;
    float fpsHistory[FPS_HISTORY] = {};
    int fpsIndex = 0;

    // GPU Timing
    ComPtr<ID3D12QueryHeap> gpuQueryHeap;
    ComPtr<ID3D12Resource> gpuReadbackBuffer;

    double gpuFrameTimeMs = 0.0;
    bool gpuTimerReady = false;

    // Memory
    uint64_t gpuMemoryMB = 0;
    uint64_t systemMemoryMB = 0;

    bool showGrid = true;
    bool showAxis = true;
private:
    // Layout
    void drawDockspace();
    void drawMenuBar();

    // Windows
    void drawHierarchy();
    void drawHierarchyNode(GameObject* go);
    void drawInspector();
    void drawConsole();
    void drawViewport();
    void drawPerformanceWindow();
    void drawExercises();

    // Rendering
    void renderViewportToTexture(ID3D12GraphicsCommandList* cmd);
    void drawViewportOverlay();

    // Helpers
    void updateFPS();
    void updateMemory();
    void log(const char* text, const ImVec4& color = ImVec4(1, 1, 1, 1));
};