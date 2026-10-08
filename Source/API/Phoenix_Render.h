#pragma once
// Per-room render overrides for scripts (occlusion fade, x-ray).

#include "API/Phoenix_Types.h"

namespace Phoenix {

/// Runtime control of the occlusion fade (dithered hole cut in walls between the camera and the player) and the
/// x-ray silhouette (tagged characters drawn through geometry). These override the scene's saved settings
/// (Post Process panel > Occlusion & X-Ray) and are cleared whenever a scene loads or Play mode stops, so a camera
/// zone script can set them per room without leaking into the next scene.
struct Render {
    static void SetOcclusionFade(bool enabled);
    static void SetOcclusionFadeRadius(float radius);
    /// By default the focus is the first object tagged with the first x-ray tag ("Player") plus the scene's focus
    /// height. With use = true, worldPos is the focus point itself (e.g. the chest); the floor cut-off sits focus
    /// height below it.
    static void SetOcclusionFocusOverride(bool use, Vec3 worldPos = Vec3(0.f, 0.f, 0.f));
    static void SetXRayEnabled(bool enabled);

    /// Drops every override above, back to the scene's settings.
    static void ResetOverrides();
};

} // namespace Phoenix
