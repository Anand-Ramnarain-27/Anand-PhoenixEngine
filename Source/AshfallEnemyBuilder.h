#pragma once
// Editor tools that build the Ashfall enemy prefabs and wire up the Kraug boss arena.

#include <string>

class SceneGraph;
class SceneManager;
class HotReloadManager;

/// GameObject > Ashfall > Build Enemy Prefabs (editor only). Builds the Act 1 enemy prefabs - Goblin, Orc,
/// Orc_Drummer, Orc_HyenaRider - like Build Hub NPC Prefabs: each imports its character glTF exactly like a drag-in
/// (ResourceModel::spawnIntoScene), loads its state machine (Assets/StateMachines/<model>.json), tags the root
/// "Enemy" (characters and enemies don't collide; the camera's spring arm passes through the mesh nodes, which are
/// tagged CameraIgnore), adds the EnemyBase script with its ConfigPath (Assets/Enemies/<id>.json), saves it through
/// PrefabManager::createPrefab and removes it from the scene again. Kraug (the Act 1 boss) is the Troll model at a
/// larger scale with kraug.json. The Goblin Hexer is the Goblin prefab with
/// goblin_hexer.json (the encounter file or a spawner sets the config). Overwrites existing prefabs of those names.
/// Returns false and says why if the EnemyBase script isn't loaded (build GameScript.dll first) or a model is missing.
bool BuildAshfallEnemyPrefabs(SceneGraph* scene, HotReloadManager* hotReload, std::string& outMessage);

/// GameObject > Ashfall > Set Up Kraug Arena (editor only, AF_KraugsDen open). One-time scene wiring for the boss fight
/// (docs/ENEMIES.md, "Kraug Ironjaw"):
///   - the two T5 cracked pillars (T5_CrackedPillar_W / _E) get the BreakablePillar script, pointed at their crack
///     placeholder and rim light (hidden when the pillar collapses);
///   - Exit_DenBack (to Act 2) requires AF_KraugsDen cleared and no longer marks it cleared (no skipping Kraug);
///   - the changed objects' prefab instances are applied to their prefabs (the editor's Apply path), and the scene saved.
/// Safe to run again (it updates what is there). Refuses other scenes; needs BreakablePillar in GameScript.dll.
bool SetUpAshfallKraugArena(SceneManager* sceneManager, HotReloadManager* hotReload, std::string& outMessage);
