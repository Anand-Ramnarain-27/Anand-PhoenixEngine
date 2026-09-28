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
    bool debugDrawUIRects = false;   // Game View: outline + pivot + anchors for every ComponentTransform2D

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

    // Silhouette drawn wherever scene geometry hides a mesh whose GameObject (or an ancestor) has one of these tags.
    struct XRayTag {
        std::string tag;
        Vector4 color = Vector4(0.35f, 0.85f, 1.0f, 1.0f);
        float fillAlpha = 0.45f;
        float outlineWidth = 1.5f;   // pixels
        bool enabled = true;
    };
    static constexpr int kMaxXRayGroups = 4;

    struct XRay {
        bool enabled = true;
        std::vector<XRayTag> tags = {
            { "Player", Vector4(0.35f, 0.85f, 1.0f, 1.0f), 0.45f, 1.5f, true },
            { "Enemy",  Vector4(1.0f, 0.25f, 0.2f, 1.0f),  0.45f, 1.5f, false },
        };
    } xray;

    // Dithered hole cut in opaque geometry between the camera and the focus (first object with xray.tags[0]).
    struct OcclusionFade {
        bool enabled = true;
        float radius = 1.6f;
        float feather = 0.5f;
        float floorClearance = 0.25f;
        float focusHeight = 1.1f;
        float coneNearScale = 0.35f;   // radius multiplier at the camera end (keeps the hole a steady size on screen)
        bool previewInSceneView = false;
    } occlusionFade;
};

// Script-set overrides layered on top of the scene's saved settings (Phoenix::Render). Cleared on scene load and
// when Play mode stops.
struct RenderOverrides {
    int occlusionEnabled = -1;       // -1 = scene setting, 0 = off, 1 = on
    float occlusionRadius = -1.0f;   // < 0 = scene setting
    bool useFocusOverride = false;
    Vector3 focusOverride = Vector3::Zero;
    int xrayEnabled = -1;            // -1 = scene setting, 0 = off, 1 = on

    void reset(){ *this = RenderOverrides(); }
};
