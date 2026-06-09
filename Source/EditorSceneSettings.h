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
        float intensity = 1.0f;
    } ambient;

    float gravityY = -9.81f; // scene-global gravity acceleration (m/s²). Applied by ComponentRigidbody.

    bool debugDrawLights = false;
    float debugLightSize = 1.0f;

    bool debugDrawBounds = false; // draws AABB or sphere depending on ComponentBounds
    bool debugDrawGrid = false; // draws occupied broad-phase grid cells (cyan wireframes)

    // NOTE: debugDrawCameraFrustums and debugDrawEditorCameraRay are declared here
    // but are unused — ModuleCamera owns its own per-instance debug-draw flags
    // (debugDrawEditorFrustum, debugDrawCullFrustum, debugDrawCameraAxes, debugDrawForwardRay).
    // Kept to avoid breaking any future serialisation that references these keys.
    bool debugDrawCameraFrustums = true;
    bool debugDrawEditorCameraRay = true;

    struct Skybox
    {
        bool enabled = false;
        std::string cubemapPath;
    } skybox;
};
