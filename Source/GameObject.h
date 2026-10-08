#pragma once
// A node in the scene hierarchy: a name, a tag, a transform, child objects and a list of components.

#include "Component.h"
#include <string>
#include <vector>
#include <memory>

class ComponentTransform;
struct ID3D12GraphicsCommandList;

/// Scene node. Every GameObject has a ComponentTransform; other components are added with createComponent<T>()
/// or addComponent(). Children are owned by the SceneGraph, not by their parent. The layout of this class is
/// mirrored in GameScript's EngineDecls: don't reorder or add members without re-exporting.
class GameObject {
public:
    explicit GameObject(const std::string& name);
    ~GameObject();

    /// Updates this object's components, then its children's, when active.
    void update(float deltaTime);
    void render(ID3D12GraphicsCommandList* cmd);

    void setParent(GameObject* newParent);
    GameObject* getParent() const { return parent; }
    const std::vector<GameObject*>& getChildren() const { return children; }
    void clearChildren(){ children.clear(); }

    ComponentTransform* getTransform() const { return transform; }

    /// Constructs a T owned by this object. Only types with an explicit instantiation (GameObject.cpp,
    /// GameObjectComponentFactories.cpp) link.
    template<typename T, typename... Args>
    T* createComponent(Args&&... args);

    void addComponent(std::unique_ptr<Component> component);

    template<typename T>
    bool removeComponent();
    bool removeComponentByType(Component::Type type);

    template<typename T>
    T* getComponent() const;

    const std::vector<std::unique_ptr<Component>>& getComponents() const { return components; }

    const std::string& getName() const { return name; }
    void setName(const std::string& newName){ name = newName; }

    const std::string& getTag() const { return tag; }
    void setTag(const std::string& newTag){ tag = newTag; }

    uint32_t getUID() const { return uid; }
    bool isActive() const { return active; }
    void setActive(bool value){ active = value; }
    bool isPendingDestroy() const { return pendingDestroy; }
    void markForDestroy(){ pendingDestroy = true; }

private:
    static uint32_t generateUID();

    uint32_t uid;
    std::string name;
    std::string tag;
    bool active = true;
    bool pendingDestroy = false;
    GameObject* parent = nullptr;
    std::vector<GameObject*> children;
    std::vector<std::unique_ptr<Component>> components;
    ComponentTransform* transform = nullptr;
};
