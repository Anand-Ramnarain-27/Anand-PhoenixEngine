#pragma once
#include "Globals.h"
#include <memory>
#include <vector>
#include <string>

class GameObject;

class SceneGraph {
public:
    SceneGraph();
    ~SceneGraph();

    GameObject* getRoot() const { return root.get(); }

    GameObject* createGameObject(const std::string& name, GameObject* parent = nullptr);
    void destroyGameObject(GameObject* go);
    void update(float deltaTime);
    void clear();
    GameObject* findGameObjectByName(const std::string& name);
    GameObject* findNearestGameObjectWithTag(const std::string& tag, const Vector3& fromPosition,
                                             GameObject* exclude = nullptr);

private:
    std::unique_ptr<GameObject> root;
    std::vector<std::unique_ptr<GameObject>> objects;
};
