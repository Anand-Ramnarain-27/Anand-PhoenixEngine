#pragma once

#include "Module.h"
#include "GraphicsSamplers.h"
#include <vector>
#include <memory>
#include <imgui.h>
#include "ImGuizmo.h"

class ImGuiPass;
class ModuleTextureSampler;
class ModuleModelViewer;

class ModuleEditor : public Module
{
public:
    ModuleEditor();
    ~ModuleEditor();

    bool init() override;
    bool cleanUp() override;
    void preRender() override;
    void render() override;

    bool IsGridVisible() const { return showGrid; }
    bool IsAxisVisible() const { return showAxis; }
    GraphicsSamplers::Type GetTextureFilter() const { return currentFilter; }
    ImGuizmo::OPERATION GetCurrentGizmoOperation() const { return currentGizmoOperation; }

private:
    void imGuiDrawCommands();

private:
    bool showGrid = true;
    bool showAxis = true;
    bool showGuizmo = true;
    GraphicsSamplers::Type currentFilter = GraphicsSamplers::LINEAR_WRAP;

    bool showTextureWindow = false;
    bool showModelViewerWindow = true;
    bool showConsole = false;
    bool showFPS = false;
    bool showAbout = false;
    bool showControls = false;

    static const int FPS_HISTORY_SIZE = 100;
    std::vector<float> fps_log;
    std::vector<float> ms_log;
    int update_counter = 0;
    const int UPDATE_INTERVAL = 30;

    std::vector<std::string> logBuffer;

    std::unique_ptr<ImGuiPass> imguiPass;
    ID3D12DescriptorHeap* imguiHeap = nullptr;

    ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
};