#pragma once
#include "ModuleD3D12.h"
#include <imgui.h>
#include "ImGuizmo.h"

struct EditorSceneSettings {
    bool showGrid = true;
    bool showAxis = true;
    bool showGizmo = false;
    ImGuizmo::OPERATION gizmoOperation = ImGuizmo::TRANSLATE;

    struct Ambient {
        Vector3 color = Vector3(0.1f, 0.1f, 0.1f);
        float intensity = 1.0f;
    } ambient;

    float gravityY = -9.81f;

    bool debugDrawLights = false;
    float debugLightSize = 1.0f;

    bool debugDrawBounds = false;
    bool debugDrawGrid = false;

    bool debugDrawCameraFrustums = true;
    bool debugDrawEditorCameraRay = true;

    struct Skybox {
        bool enabled = false;
        std::string cubemapPath;
    } skybox;
};
