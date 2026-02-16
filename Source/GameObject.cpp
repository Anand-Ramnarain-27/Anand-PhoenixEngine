#include "Globals.h"
#include "GameObject.h"
#include "Component.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include <algorithm>

GameObject::GameObject(const std::string& name)
    : name(name)
{
    transform = createComponent<ComponentTransform>();
}

GameObject::~GameObject() = default;

void GameObject::setParent(GameObject* newParent)
{
    if (parent == newParent)
        return;

    if (parent)
    {
        auto& siblings = parent->children;

        siblings.erase(
            std::remove(siblings.begin(), siblings.end(), this),
            siblings.end()
        );
    }

    parent = newParent;

    if (parent)
        parent->children.push_back(this);

    transform->markDirty();
}

void GameObject::update(float deltaTime)
{
    for (auto& c : components)
        c->update(deltaTime);

    for (auto* child : children)
        child->update(deltaTime);
}

void GameObject::render(ID3D12GraphicsCommandList* cmd)
{
    for (auto& c : components)
        c->render(cmd);

    for (auto* child : children)
        child->render(cmd);
}

template<typename T, typename... Args>
T* GameObject::createComponent(Args&&... args)
{
    auto component = std::make_unique<T>(this, std::forward<Args>(args)...);
    T* ptr = component.get();
    components.push_back(std::move(component));
    return ptr;
}

template ComponentTransform* GameObject::createComponent<ComponentTransform>();
template ComponentMesh* GameObject::createComponent<ComponentMesh>();