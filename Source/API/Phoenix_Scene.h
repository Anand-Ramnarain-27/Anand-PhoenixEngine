#pragma once
#include "API/Phoenix_Types.h"
#include <string>

class GameObject;

namespace Phoenix {

struct Scene {
    static GameObject* Find(const std::string& name);

    static GameObject* Spawn(const std::string& name);

    static void Destroy(GameObject* go);

    // Replaces the active scene at the end of this frame's update (safe to call from a script's Update(),
    // which is destroyed along with everything else). Accepts a scene name ("AF_KraugsDen", looked up in
    // Library/Scenes/) or a path to a scene .json. Play mode keeps running in the new scene.
    static void LoadScene(const std::string& sceneNameOrPath);
    static bool SceneExists(const std::string& sceneNameOrPath);

    // Instantiates a prefab (by name, from Library/Prefabs/) at the end of this frame's update. Queued
    // spawns are dropped if a LoadScene() is processed in the same frame.
    static void InstantiatePrefab(const std::string& prefabName, Vec3 position, Quat rotation = Quat::Identity);

    // File of the scene that is currently loaded ("" if unsaved), and just its name ("AF_KraugsDen").
    static std::string GetActiveScenePath();
    static std::string GetActiveSceneName();

    // The camera the Game view renders through (a GameObject with a Camera component).
    static GameObject* GetActiveCamera();
    static void SetActiveCamera(GameObject* cameraObject);

    // Vertical field of view of the active camera, in radians. Get returns 0 when there is no active camera;
    // Set is ignored then, and clamps to a sane range otherwise. Not saved: the camera's authored FOV comes back
    // when the scene reloads.
    static float GetActiveCameraFOV();
    static void SetActiveCameraFOV(float radians);
};

} // namespace Phoenix
