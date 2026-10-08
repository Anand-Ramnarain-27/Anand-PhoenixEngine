#pragma once
// Creates components by Component::Type (scene and prefab loading, the Add Component menu).

#include "Component.h"
#include <memory>

class GameObject;

class ComponentFactory {
public:
    /// Null for an unknown type.
    static std::unique_ptr<Component> CreateComponent(Component::Type type, GameObject* owner);
};
