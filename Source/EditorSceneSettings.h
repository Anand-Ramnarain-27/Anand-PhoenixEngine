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

    struct DirectionalLight
    {
        Vector3 direction = Vector3(-0.5f, -1.0f, -0.5f);
        Vector3 color = Vector3(1.0f, 1.0f, 1.0f);
        float intensity = 1.0f;
        bool enabled = true;
    } directionalLight[2];

    struct PointLight
    {
        Vector3 position = Vector3(0.0f, 2.0f, 0.0f);
        Vector3 color = Vector3(1.0f, 1.0f, 1.0f);
        float intensity = 1.0f;
        float radius = 5.0f;
        bool enabled = false;
    } pointLight;

    struct SpotLight
    {
        Vector3 position = Vector3(0.0f, 3.0f, 0.0f);
        Vector3 direction = Vector3(0.0f, -1.0f, 0.0f);
        Vector3 color = Vector3(1.0f, 1.0f, 1.0f);
        float intensity = 1.0f;
        float innerAngle = 15.0f; 
        float outerAngle = 30.0f; 
        float radius = 10.0f;
        bool enabled = false;
    } spotLight;

    bool debugDrawLights = false;
    float debugLightSize = 1.0f;
};