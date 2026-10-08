#pragma once
// Editor tool that builds the Ashfall combat HUD prefab from its layout files.

#include <string>

class SceneGraph;
class HotReloadManager;

/// Editor-only: builds the Ashfall combat HUD (a Canvas hierarchy whose node names all start with "HUD_") in
/// `scene`, attaches the AshfallHUD script to its root, saves it as Library/Prefabs/AshfallHUD.prefab and then
/// removes the built hierarchy again, so no static copy is left in the scene. Geometry comes from
/// Assets/Ashfall_UI/hud_layout.json and colours from ui_tokens.json; textures are the Ashfall_UI pack's.
///
/// Nothing is pushed to the undo stack. Returns false (and says why in `outMessage`) if the AshfallHUD script
/// class isn't loaded or the prefab couldn't be written; in the latter case the built hierarchy is left in place.
bool BuildAshfallHUDPrefab(SceneGraph* scene, HotReloadManager* hotReload, std::string& outMessage);

inline constexpr const char* kAshfallHUDPrefab = "AshfallHUD";
