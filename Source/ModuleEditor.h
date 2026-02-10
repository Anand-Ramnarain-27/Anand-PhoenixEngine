#pragma once

#include "Module.h"
#include "ImGuiPass.h"
#include "ShaderTableDesc.h"
#include "RenderTexture.h"
#include "DebugDrawPass.h"
#include "IScene.h"

#include <memory>
#include <imgui.h>

class ModuleCamera;

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
    struct ConsoleEntry
    {
        std::string text;
        ImVec4 color;
    };
    std::vector<ConsoleEntry> console;
    bool autoScrollConsole = true;

    std::unique_ptr<ImGuiPass> imguiPass;
    ShaderTableDesc descTable;

    bool showEditor = true;
    bool firstFrame = true;

    ImVec2 viewportSize = { 0,0 };
    ImVec2 viewportPos = { 0,0 };

    bool showFPSWindow = false;

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

    std::unique_ptr<RenderTexture> viewportRT;
    std::unique_ptr<DebugDrawPass> debugDrawPass;

    bool showGrid = true;
    bool showAxis = true;

    std::unique_ptr<IScene> activeScene;

private:
    void drawMenuBar();
    void drawDockspace();
    void drawEditorPanel();
    void drawExerciseList();
    void drawViewport();

    void log(const char* text, const ImVec4& color = ImVec4(1, 1, 1, 1));
    void drawConsole();

    void updateFPS();
    void drawFPSWindow();

    void updateMemory();

    void drawCameraStats();

    void renderViewportToTexture(ID3D12GraphicsCommandList* cmd);
    void drawViewportOverlay();
};
