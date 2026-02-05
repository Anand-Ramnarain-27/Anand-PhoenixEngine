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
    // UI
    std::unique_ptr<ImGuiPass> imguiPass;
    ShaderTableDesc descTable;

    // Layout
    bool showEditor = true;
    bool firstFrame = true;

    // Viewport
    ImVec2 viewportSize = { 0,0 };
    ImVec2 viewportPos = { 0,0 };

private:
    void drawMenuBar();
    void drawDockspace();
    void drawEditorPanel();
    void drawExerciseList();
    void drawViewport();
};
