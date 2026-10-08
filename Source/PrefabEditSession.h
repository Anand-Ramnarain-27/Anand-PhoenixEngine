#pragma once
// State of the editor's prefab edit mode (a prefab opened on its own in an isolated scene).

#include "SceneGraph.h"
#include <string>
#include <memory>

class GameObject;

struct PrefabEditSession {
    bool active = false;
    std::string prefabName;
    std::unique_ptr<SceneGraph> isolatedScene;
    GameObject* rootObject = nullptr;

    void clear(){
        active = false;
        prefabName.clear();
        rootObject = nullptr;
        isolatedScene.reset();
    }
};
