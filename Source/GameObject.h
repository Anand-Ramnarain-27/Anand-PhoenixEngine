#pragma once

#include "UUID64.h"
#include <string>
#include <vector>
#include <memory>

class Component;
class ComponentTransform;

class GameObject
{
public:
    explicit GameObject(const std::string& name);
    ~GameObject();

    void update(float deltaTime);

    void setParent(GameObject* newParent);
    GameObject* getParent() const { return parent; }
    const std::vector<GameObject*>& getChildren() const { return children; }

    ComponentTransform* getTransform() const { return transform; }

    template<typename T, typename... Args>
    T* createComponent(Args&&... args);

    const std::string& getName() const { return name; }
    void setName(const std::string& newName) { name = newName; }

    // UUID System
    UUID64 getUUID() const { return uuid; }

    // Active state
    bool isActive() const { return active; }
    void setActive(bool value) { active = value; }

private:
    UUID64 uuid;                                      // Unique identifier
    std::string name;
    bool active = true;

    GameObject* parent = nullptr;
    std::vector<GameObject*> children;

    std::vector<std::unique_ptr<Component>> components;

    ComponentTransform* transform = nullptr;
};
