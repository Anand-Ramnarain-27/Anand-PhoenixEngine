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
#include "ComponentBillboard.h"
#include "ComponentTransform2D.h"
#include "ComponentCanvas.h"
#include "ComponentImage.h"
#include "ComponentLabel.h"
#include "ComponentButton.h"
#include "ComponentProgressBar.h"
#include "ComponentCheckBox.h"
#include "ComponentSlider.h"
#include "ComponentParticleSystem.h"
#include "ComponentTrail.h"
#include "ComponentAIAgent.h"

std::unique_ptr<Component> ComponentFactory::CreateComponent(Component::Type type, GameObject* owner){
    switch (type){
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
    case Component::Type::Bounds: return std::make_unique<ComponentBounds>(owner);
    case Component::Type::Decal: return std::make_unique<ComponentDecal>(owner);
    case Component::Type::Billboard: return std::make_unique<ComponentBillboard>(owner);
    case Component::Type::ParticleSystem: return std::make_unique<ComponentParticleSystem>(owner);
        case Component::Type::Trail: return std::make_unique<ComponentTrail>(owner);
    case Component::Type::AIAgent: return std::make_unique<ComponentAIAgent>(owner);
    case Component::Type::Transform2D: return std::make_unique<ComponentTransform2D>(owner);
    case Component::Type::Canvas: return std::make_unique<ComponentCanvas>(owner);
    case Component::Type::Image: return std::make_unique<ComponentImage>(owner);
    case Component::Type::Label: return std::make_unique<ComponentLabel>(owner);
    case Component::Type::Button: return std::make_unique<ComponentButton>(owner);
    case Component::Type::ProgressBar: return std::make_unique<ComponentProgressBar>(owner);
    case Component::Type::CheckBox: return std::make_unique<ComponentCheckBox>(owner);
    case Component::Type::Slider: return std::make_unique<ComponentSlider>(owner);
    default: LOG("ComponentFactory: Unknown component type %d", (int)type); return nullptr;
    }
}
