#include "Globals.h"
#include "GameObject.h"
#include "Component.h"
#include "ComponentTransform.h"
#include <algorithm>

GameObject::GameObject(const std::string& name)
    : uuid(UUID64::Generate())  // Generate UUID automatically
    , name(name)
{
    transform = createComponent<ComponentTransform>();
}

GameObject::~GameObject() = default;

void GameObject::setParent(GameObject* newParent)
{
    if (parent == newParent)
        return;

    // Remove from old parent
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
    if (!active)
        return;

    for (auto& c : components)
        c->update(deltaTime);

    for (auto* child : children)
        child->update(deltaTime);
}

template<typename T, typename... Args>
T* GameObject::createComponent(Args&&... args)
{
    auto component = std::make_unique<T>(this, std::forward<Args>(args)...);
    T* ptr = component.get();
    components.push_back(std::move(component));
    return ptr;
}

// Explicit template instantiation
template ComponentTransform* GameObject::createComponent<ComponentTransform>();
