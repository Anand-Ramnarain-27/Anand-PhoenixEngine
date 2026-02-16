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
    void render(ID3D12GraphicsCommandList* cmd);

    void setParent(GameObject* newParent);
    GameObject* getParent() const { return parent; }
    const std::vector<GameObject*>& getChildren() const { return children; }

    ComponentTransform* getTransform() const { return transform; }

    // Create component (used during scene construction)
    template<typename T, typename... Args>
    T* createComponent(Args&&... args);

    // Add pre-created component (used during deserialization)
    void addComponent(std::unique_ptr<Component> component);

    // Component access for serialization
    const std::vector<std::unique_ptr<Component>>& getComponents() const { return components; }

    // Get component by type
    template<typename T>
    T* getComponent() const;

    // Clear all children references (for scene clearing)
    void clearChildren() { children.clear(); }

    const std::string& getName() const { return name; }
    void setName(const std::string& newName) { name = newName; }

    // Serialization support
    uint32_t getUID() const { return uid; }
    bool isActive() const { return active; }
    void setActive(bool value) { active = value; }

private:
    uint32_t uid;
    std::string name;
    bool active = true;

    GameObject* parent = nullptr;
    std::vector<GameObject*> children;

    std::vector<std::unique_ptr<Component>> components;

    ComponentTransform* transform = nullptr;

    static uint32_t generateUID();
};