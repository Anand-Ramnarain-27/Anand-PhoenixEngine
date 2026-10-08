#pragma once
// Per-scene settings (saved with the scene) and the runtime state scripts layer on top of them.

#include "ModuleD3D12.h"
#include <imgui.h>
#include "ImGuizmo.h"

/// A scene's settings. The look (skybox, ambient, post-process, fog, x-ray, occlusion fade) and gravity are saved
/// with the scene by SceneSerializer, whose JSON keys are part of the scene format; the editor's view and
/// debug-draw toggles live here too but are session-only.
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

    /// Silhouette drawn wherever scene geometry hides a mesh whose GameObject (or an ancestor) has one of these tags.
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

    /// Dithered hole cut in opaque geometry between the camera and the focus (first object with xray.tags[0]).
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

/// Script-set overrides layered on top of the scene's saved settings (Phoenix::Render). Cleared on scene load and
/// when Play mode stops.
struct RenderOverrides {
    int occlusionEnabled = -1;       // -1 = scene setting, 0 = off, 1 = on
    float occlusionRadius = -1.0f;   // < 0 = scene setting
    bool useFocusOverride = false;
    Vector3 focusOverride = Vector3::Zero;
    int xrayEnabled = -1;            // -1 = scene setting, 0 = off, 1 = on

    void reset(){ *this = RenderOverrides(); }
};

class GameObject;
class Component;

/// Engine functions handed to GameScript.dll through SceneManager (set by RuntimeCore::init). Component
/// constructors and model loading live in the engine executable, not in PhoenixCore.lib, so a script can't call
/// them directly; it calls these pointers instead (Phoenix::VFX).
struct EngineHooks {
    Component* (*addComponent)(GameObject* owner, int componentType) = nullptr;   // Component::Type; existing one is returned
    bool (*loadModel)(GameObject* owner, const char* assetPath) = nullptr;        // adds/reuses a ComponentMesh
    void (*logWarning)(const char* text) = nullptr;                               // editor Console (null in the player)
};

/// Game-time scaling (Phoenix::VFX hit-stop / slow-mo). RuntimeCore::tick multiplies the frame delta by timeScale
/// before the scene, scripts, collision and animation see it; the unscaled delta stays readable for things that
/// must keep real time (HUD, camera shake, the hit-stop timer itself). timeScale returns to 1 on scene load and
/// when Play mode stops.
struct RuntimeTime {
    float timeScale = 1.0f;
    float unscaledDeltaTime = 0.0f;   // this frame, before scaling
    float scaledTime = 0.0f;          // sum of scaled deltas (drives VFX UV scroll)
    float unscaledTime = 0.0f;
};
