#include "Globals.h"
#include "GameObject.h"
#include "Component.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ComponentCamera.h"
#include "ComponentDirectionalLight.h"
#include "ComponentPointLight.h"
#include "ComponentSpotLight.h"
#include <algorithm>
#include <random>

GameObject::GameObject(const std::string& name)
    : uid(generateUID()), name(name)
{
    transform = createComponent<ComponentTransform>();
}

GameObject::~GameObject() = default;

uint32_t GameObject::generateUID()
{
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<uint32_t> dis;
    return dis(gen);
}

void GameObject::setParent(GameObject* newParent)
{
    if (parent == newParent) return;

    if (parent)
    {
        auto& s = parent->children;
        s.erase(std::remove(s.begin(), s.end(), this), s.end());
    }

    parent = newParent;
    if (parent) parent->children.push_back(this);
    transform->markDirty();
}

void GameObject::update(float deltaTime)
{
    if (!active) return;
    for (auto& c : components) c->update(deltaTime);
    for (auto* child : children) child->update(deltaTime);
}

void GameObject::render(ID3D12GraphicsCommandList* cmd)
{
    if (!active) return;
    for (auto& c : components) c->render(cmd);
    for (auto* child : children) child->render(cmd);
}

template<typename T, typename... Args>
T* GameObject::createComponent(Args&&... args)
{
    auto comp = std::make_unique<T>(this, std::forward<Args>(args)...);
    T* ptr = comp.get();
    components.push_back(std::move(comp));
    return ptr;
}

void GameObject::addComponent(std::unique_ptr<Component> component)
{
    if (component) components.push_back(std::move(component));
}

template<typename T>
T* GameObject::getComponent() const
{
    for (const auto& c : components)
        if (auto* p = dynamic_cast<T*>(c.get())) return p;
    return nullptr;
}

template<typename T>
bool GameObject::removeComponent()
{
    for (auto it = components.begin(); it != components.end(); ++it)
    {
        if (dynamic_cast<T*>(it->get()) && (*it)->getType() != Component::Type::Transform)
        {
            components.erase(it);
            return true;
        }
    }
    return false;
}

bool GameObject::removeComponentByType(Component::Type type)
{
    if (type == Component::Type::Transform)
    {
        LOG("GameObject: Cannot remove Transform component.");
        return false;
    }

    for (auto it = components.begin(); it != components.end(); ++it)
    {
        if ((*it)->getType() == type)
        {
            components.erase(it);
            return true;
        }
    }
    return false;
}

template ComponentTransform* GameObject::createComponent<ComponentTransform>();
template ComponentMesh* GameObject::createComponent<ComponentMesh>();
template ComponentCamera* GameObject::createComponent<ComponentCamera>();
template ComponentDirectionalLight* GameObject::createComponent<ComponentDirectionalLight>();
template ComponentPointLight* GameObject::createComponent<ComponentPointLight>();
template ComponentSpotLight* GameObject::createComponent<ComponentSpotLight>();

template ComponentTransform* GameObject::getComponent<ComponentTransform>() const;
template ComponentMesh* GameObject::getComponent<ComponentMesh>() const;
template ComponentCamera* GameObject::getComponent<ComponentCamera>() const;
template ComponentDirectionalLight* GameObject::getComponent<ComponentDirectionalLight>() const;
template ComponentPointLight* GameObject::getComponent<ComponentPointLight>() const;
template ComponentSpotLight* GameObject::getComponent<ComponentSpotLight>() const;

template bool GameObject::removeComponent<ComponentMesh>();
template bool GameObject::removeComponent<ComponentCamera>();
template bool GameObject::removeComponent<ComponentDirectionalLight>();
template bool GameObject::removeComponent<ComponentPointLight>();
template bool GameObject::removeComponent<ComponentSpotLight>();