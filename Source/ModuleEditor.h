#pragma once

#include "Module.h"
#include "ImGuiPass.h"
#include "ShaderTableDesc.h"

#include <memory>
#include <imgui.h>

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

};
