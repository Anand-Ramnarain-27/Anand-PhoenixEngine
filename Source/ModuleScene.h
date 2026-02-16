#pragma once

#include <memory>
#include <vector>
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

    // For serialization: clear all objects except root
    void clear();

    GameObject* findGameObjectByName(const std::string& name);

private:
    std::unique_ptr<GameObject> root;
    std::vector<std::unique_ptr<GameObject>> objects;
};