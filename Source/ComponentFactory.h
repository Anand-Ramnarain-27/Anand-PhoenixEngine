#pragma once

#include "Component.h"
#include <memory>

class GameObject;

class ComponentFactory
{
public:
    static std::unique_ptr<Component> CreateComponent(Component::Type type, GameObject* owner);
};