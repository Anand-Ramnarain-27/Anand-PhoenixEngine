#pragma once

#include "UUID64.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

class GameObject;

class ModuleScene
{
public:
    ModuleScene();
    ~ModuleScene();

    GameObject* getRoot() const { return root.get(); }

    GameObject* createGameObject(
        const std::string& name,
        GameObject* parent = nullptr
    );

    void update(float deltaTime);

    // UUID Lookup - essential for scene serialization
    GameObject* findGameObjectByUUID(const UUID64& uuid) const;

    // Get all GameObjects (useful for serialization)
    const std::vector<std::unique_ptr<GameObject>>& getAllGameObjects() const { return objects; }

    // Clear all GameObjects (useful when loading a new scene)
    void clear();

private:
    // Update the UUID map when GameObjects are created
    void registerGameObject(GameObject* go);

private:
    std::unique_ptr<GameObject> root;
    std::vector<std::unique_ptr<GameObject>> objects;

    // Fast UUID lookup
    std::unordered_map<UUID64, GameObject*> uuidMap;
};
