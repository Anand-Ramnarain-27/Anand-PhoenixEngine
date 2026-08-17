#pragma once
#include <string>

class GameObject;

namespace Phoenix {

struct Scene {
    static GameObject* Find(const std::string& name);

    static GameObject* Spawn(const std::string& name);

    static void Destroy(GameObject* go);
};

} // namespace Phoenix
