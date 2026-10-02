#pragma once
#include <functional>
#include <string>

class GameObject;
class HotReloadManager;
class SceneManager;

// GameObject > Ashfall > Build VFX Test Scene (editor only). Turns the active scene into VFX_Test and saves it as
// Library/Scenes/VFX_Test.json: a dim directional light, a fixed camera looking down at the trigger grid, a floor
// (`makeFloor`, called only once the checks pass: the editor's plain primitive cube) and a VFX_TestBench object carrying the
// VfxTestBench script, which lays out one trigger per recipe at Play and fires them (docs/VFX.md, "VFX_Test").
//
// Only runs on a new, unsaved scene or on VFX_Test itself (an earlier build is replaced), so it can never write
// over a level. Returns false and says why in `outMessage`: script class not loaded (build GameScript.dll first),
// wrong scene, or the save failed.
bool BuildAshfallVfxTestScene(SceneManager* sceneManager, HotReloadManager* hotReload,
                              const std::function<GameObject*()>& makeFloor, std::string& outMessage);

inline constexpr const char* kAshfallVfxTestScene = "VFX_Test";
