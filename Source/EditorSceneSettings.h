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
    bool debugDrawNav = false;

    bool debugDrawCameraFrustums = true;
    bool debugDrawEditorCameraRay = true;

    struct Skybox {
        bool enabled = false;
        std::string cubemapPath;
    } skybox;

    struct PostProcess {
        float exposure = 0.0f;
        bool bloomEnabled = true;
        float bloomThreshold = 1.0f;
        float bloomIntensity = 0.6f;
        bool lutEnabled = false;
        std::string lutPath;
    } postProcess;

    struct Fog {
        enum class Mode { Linear = 0, ExponentialHeight = 1, Volumetric = 2 };

        bool enabled = false;
        Mode mode = Mode::Linear;
        Vector3 color = Vector3(0.5f, 0.55f, 0.6f);
        float startDistance = 10.0f;
        float endDistance = 100.0f;
        float maxOpacity = 1.0f;
        float density = 0.02f;
        float heightFalloff = 0.1f;
        float heightOffset = 0.0f;

        int numSteps = 32;
        float extinctionCoeff = 0.15f;
        float noiseAmount = 0.5f;
        float fogIntensity = 1.0f;
        float anisotropyG = 0.3f;
        bool halfResolution = true;
        bool boundedRayLength = false;
    } fog;
};
