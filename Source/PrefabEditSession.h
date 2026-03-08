#pragma once
#include "ModuleScene.h"
#include <string>
#include <memory>

class GameObject;

struct PrefabEditSession {
    bool active = false;
    std::string prefabName;
    std::unique_ptr<ModuleScene> isolatedScene;
    GameObject* rootObject = nullptr;

    void clear() {
        active = false;
        prefabName.clear();
        rootObject = nullptr;
        isolatedScene.reset();
    }
};