#pragma once
#include "ModuleD3D12.h"
#include <imgui.h>
#include "ImGuizmo.h"

struct EditorSceneSettings
{
    bool showGrid = true;
    bool showAxis = true;
    bool showGizmo = false;
    ImGuizmo::OPERATION gizmoOperation = ImGuizmo::TRANSLATE;

    struct Ambient
    {
        Vector3 color = Vector3(0.1f, 0.1f, 0.1f);
        float   intensity = 1.0f;
    } ambient;

    bool  debugDrawLights = false;
    float debugLightSize = 1.0f;

    bool  debugDrawCameraFrustums = true;
    bool  debugDrawEditorCameraRay  = true;
};