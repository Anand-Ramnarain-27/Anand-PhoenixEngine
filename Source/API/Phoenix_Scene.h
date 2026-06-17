#pragma once
#include <string>

class GameObject;

namespace Phoenix {

struct Scene {
    // Find a GameObject in the active scene by name (first match).
    // Returns nullptr if not found.
    static GameObject* Find(const std::string& name);

    // Create a new empty GameObject in the active scene.
    static GameObject* Spawn(const std::string& name);

    // Mark a GameObject for destruction at end of frame.
    static void Destroy(GameObject* go);
};

} // namespace Phoenix
