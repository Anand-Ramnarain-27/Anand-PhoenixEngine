#include "Globals.h"
#include "ComponentFactory.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ComponentCamera.h"
#include "ComponentDirectionalLight.h"
#include "ComponentPointLight.h"
#include "ComponentSpotLight.h"

std::unique_ptr<Component> ComponentFactory::CreateComponent(Component::Type type, GameObject* owner)
{
    switch (type)
    {
    case Component::Type::Transform: return std::make_unique<ComponentTransform>(owner);
    case Component::Type::Mesh: return std::make_unique<ComponentMesh>(owner);
    case Component::Type::Camera: return std::make_unique<ComponentCamera>(owner);
    case Component::Type::DirectionalLight: return std::make_unique<ComponentDirectionalLight>(owner);
    case Component::Type::PointLight: return std::make_unique<ComponentPointLight>(owner);
    case Component::Type::SpotLight: return std::make_unique<ComponentSpotLight>(owner);
    default:
        LOG("ComponentFactory: Unknown component type %d", (int)type);
        return nullptr;
    }
}