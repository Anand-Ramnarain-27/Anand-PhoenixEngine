#pragma once
// Editor tools that build the Ashfall hub's NPC prefabs and its UI pages prefab.

#include <string>

class SceneGraph;
class HotReloadManager;

// Editor-only builders for the Ashfall Upper Courtyard hub. Both work like BuildAshfallHUDPrefab: build a hierarchy
// in `scene`, save it through PrefabManager::createPrefab, then remove it again so nothing is left in the scene.
// Nothing is pushed to the undo stack. They return false and say why in `outMessage` if a script class isn't loaded
// (build GameScript.dll first), an asset is missing, or a prefab couldn't be written.

/// GameObject > Ashfall > Build Hub NPC Prefabs: NPC_Bruck / NPC_Mira / NPC_Raniver / NPC_Oskar. Each imports its
/// character glTF exactly like a drag-in (ResourceModel::spawnIntoScene: root, Armature at 0.01, skinned mesh, bones,
/// ComponentAnimation), sets the NPC's state machine (idle by default), adds the HubNPC script with its NpcId and
/// tags the mesh nodes CameraIgnore. The root stays untagged: character collision ignores anything under a tagged
/// top-level object, and the NPCs must be solid.
bool BuildAshfallHubNPCPrefabs(SceneGraph* scene, HotReloadManager* hotReload, std::string& outMessage);

/// GameObject > UI > Build Hub Pages Prefab: the AshfallHubPages prefab (interact prompt + the Forge, Mira, Raniver
/// and Oskar pages), built node by node from Assets/Ashfall_UI/hub_pages_layout.json with the HubPages script on
/// its root. Colours may name ui_tokens.json colours.
bool BuildAshfallHubPagesPrefab(SceneGraph* scene, HotReloadManager* hotReload, std::string& outMessage);

inline constexpr const char* kAshfallHubPagesPrefab = "AshfallHubPages";
