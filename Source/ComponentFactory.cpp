#include "Globals.h"
#include "ComponentFactory.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ComponentCamera.h"
#include "ComponentLights.h"
#include "ComponentScript.h"
#include "ComponentAnimation.h"
#include "ComponentCharacterMotion.h"
#include "ComponentSimpleCharacterController.h"
#include "ComponentRigidbody.h"
#include "ComponentBounds.h"
#include "ComponentDecal.h"
#include "ComponentParticleEmitter.h"

std::unique_ptr<Component> ComponentFactory::CreateComponent(Component::Type type, GameObject* owner) {
    switch (type) {
    case Component::Type::Transform: return std::make_unique<ComponentTransform>(owner);
    case Component::Type::Mesh: return std::make_unique<ComponentMesh>(owner);
    case Component::Type::Camera: return std::make_unique<ComponentCamera>(owner);
    case Component::Type::DirectionalLight: return std::make_unique<ComponentDirectionalLight>(owner);
    case Component::Type::PointLight: return std::make_unique<ComponentPointLight>(owner);
    case Component::Type::SpotLight: return std::make_unique<ComponentSpotLight>(owner);
    case Component::Type::Script: return std::make_unique<ComponentScript>(owner);
    case Component::Type::Animation: return std::make_unique<ComponentAnimation>(owner);
    case Component::Type::CharacterMotion: return std::make_unique<ComponentCharacterMotion>(owner);
    case Component::Type::SimpleCharacterController: return std::make_unique<ComponentSimpleCharacterController>(owner);
    case Component::Type::Rigidbody: return std::make_unique<ComponentRigidbody>(owner);
    case Component::Type::Bounds:    return std::make_unique<ComponentBounds>(owner);
    case Component::Type::Decal:           return std::make_unique<ComponentDecal>(owner);
    case Component::Type::ParticleEmitter: return std::make_unique<ComponentParticleEmitter>(owner);
    default: LOG("ComponentFactory: Unknown component type %d", (int)type); return nullptr;
    }
}
