#pragma once
// Editor tool that wires a level's traps, puzzles, secrets and gates from Assets/Traps/setup/<Level>.setup.json.

#include <string>

class SceneManager;
class HotReloadManager;

/// GameObject > Ashfall > Set Up Traps & Puzzles (editor only, an Ashfall level open). Applies the open level's
/// setup file, Assets/Traps/setup/<Level>.setup.json (Level = the scene name without "AF_"), to the scene:
///   "rename"  - gives duplicate-named pieces unique names: { "path": "<parent path>", "name": "<old>", "index": n,
///               "to": "<new>" } renames the n-th child called <old> under that parent (skipped once done);
///   "create"  - adds missing placeholder objects: { "name", "parent": "<path>", "model": "<under Assets/>",
///               "position", "yaw" (degrees) or "rotation" [x,y,z,w], "scale", "tag" } (intermediate groups are
///               created; an object already at that path is left alone, so hand edits survive);
///   "scripts" - adds (or updates) a script component: { "object": "<path>", "class": "Trap", "fields": {...} }; the
///               fields are written as the Inspector saves them (object refs as { "path": "..." }).
/// Then every touched prefab instance is applied to its prefab (the editor's Apply) and the scene is saved.
/// Safe to run again. Nothing per level lives in C++: the setup files are the data. Refuses a scene without a setup
/// file, and script classes GameScript.dll doesn't have (build it first).
bool SetUpAshfallTrapsPuzzles(SceneManager* sceneManager, HotReloadManager* hotReload, std::string& outMessage);
