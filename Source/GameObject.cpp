#include "Globals.h"
#include "GameObject.h"
#include "Component.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ComponentCamera.h"
#include <algorithm>
#include <random>

GameObject::GameObject(const std::string& name)
    : name(name)
    , uid(generateUID())
{
    transform = createComponent<ComponentTransform>();
}

GameObject::~GameObject() = default;

uint32_t GameObject::generateUID()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis;
    return dis(gen);
}

void GameObject::setParent(GameObject* newParent)
{
    if (parent == newParent)
        return;

    if (parent)
    {
        auto& siblings = parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
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

void GameObject::addComponent(std::unique_ptr<Component> component)
{
    if (component)
    {
        components.push_back(std::move(component));
    }
}

template<typename T>
T* GameObject::getComponent() const
{
    for (auto& comp : components)
    {
        T* casted = dynamic_cast<T*>(comp.get());
        if (casted)
            return casted;
    }
    return nullptr;
}

template ComponentTransform* GameObject::createComponent<ComponentTransform>();
template ComponentMesh* GameObject::createComponent<ComponentMesh>();
template ComponentCamera* GameObject::createComponent<ComponentCamera>();

template ComponentTransform* GameObject::getComponent<ComponentTransform>() const;
template ComponentMesh* GameObject::getComponent<ComponentMesh>() const;
template ComponentCamera* GameObject::getComponent<ComponentCamera>() const;