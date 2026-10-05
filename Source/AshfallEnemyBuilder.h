#pragma once
#include <string>

class SceneGraph;
class HotReloadManager;

// GameObject > Ashfall > Build Enemy Prefabs (editor only). Builds the Act 1 enemy prefabs - Goblin, Orc,
// Orc_Drummer, Orc_HyenaRider - like Build Hub NPC Prefabs: each imports its character glTF exactly like a drag-in
// (ResourceModel::spawnIntoScene), loads its state machine (Assets/StateMachines/<model>.json), tags the root
// "Enemy" (characters and enemies don't collide; the camera's spring arm passes through the mesh nodes, which are
// tagged CameraIgnore), adds the EnemyBase script with its ConfigPath (Assets/Enemies/<id>.json), saves it through
// PrefabManager::createPrefab and removes it from the scene again. The Goblin Hexer is the Goblin prefab with
// goblin_hexer.json (the encounter file or a spawner sets the config). Overwrites existing prefabs of those names.
// Returns false and says why if the EnemyBase script isn't loaded (build GameScript.dll first) or a model is missing.
bool BuildAshfallEnemyPrefabs(SceneGraph* scene, HotReloadManager* hotReload, std::string& outMessage);
