#include "Globals.h"
#include "ComponentFactory.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ComponentCamera.h"

std::unique_ptr<Component> ComponentFactory::CreateComponent(Component::Type type, GameObject* owner)
{
    switch (type)
    {
    case Component::Type::Transform:
        return std::make_unique<ComponentTransform>(owner);

    case Component::Type::Mesh:
        return std::make_unique<ComponentMesh>(owner);

    case Component::Type::Camera:
        return std::make_unique<ComponentCamera>(owner);

    default:
        LOG("ComponentFactory: Unknown component type %d", (int)type);
        return nullptr;
    }
}