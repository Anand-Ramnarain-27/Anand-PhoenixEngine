#pragma once

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

    // Hierarchy
    void setParent(GameObject* newParent);
    GameObject* getParent() const { return parent; }
    const std::vector<GameObject*>& getChildren() const { return children; }

    // Components
    ComponentTransform* getTransform() const { return transform; }

    template<typename T, typename... Args>
    T* createComponent(Args&&... args);

    const std::string& getName() const { return name; }

private:
    std::string name;
    bool active = true;

    GameObject* parent = nullptr;
    std::vector<GameObject*> children;

    std::vector<std::unique_ptr<Component>> components;

    ComponentTransform* transform = nullptr;
};
