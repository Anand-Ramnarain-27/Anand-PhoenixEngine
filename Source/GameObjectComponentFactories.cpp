// Explicit instantiations of GameObject::createComponent<T>() for component
// types that aren't needed by a thin consumer of GameObject.cpp (PhoenixCore /
// GameScript.dll) - constructing any of these pulls in ImGui, the debug-draw
// gizmo library, AnimationController, and/or ModuleFileSystem. Compiled only
// into Engine.vcxproj and Player.vcxproj, both of which already compile every
// Component*.cpp these constructors need. See GameObject.cpp for the
// ComponentTransform (and, via getComponent<T>(), ComponentScript) instantiations
// that do live there.
//
// createComponent<T>() is redefined here (identical to GameObject.cpp's copy)
// because explicit instantiation requires the template definition to be visible
// in the instantiating translation unit - GameObject.h only declares it.
#include "Globals.h"
#include "GameObject.h"
#include "PrefabManager.h"
#include "ComponentMesh.h"
#include "ComponentCamera.h"
#include "ComponentLights.h"
#include "ComponentAnimation.h"
#include "ComponentCharacterMotion.h"
#include "ComponentSimpleCharacterController.h"
#include "ComponentRigidbody.h"
#include "ComponentBounds.h"
#include "ComponentDecal.h"
#include "ComponentBillboard.h"
#include "ComponentParticleSystem.h"
#include "ComponentTrail.h"

template<typename T, typename... Args>
T* GameObject::createComponent(Args&&... args){
    auto comp = std::make_unique<T>(this, std::forward<Args>(args)...);
    T* ptr = comp.get();
    components.push_back(std::move(comp));

    if (ptr->getType() != Component::Type::Transform){
        PrefabManager::markComponentAdded(this, static_cast<int>(ptr->getType()));
    }

    return ptr;
}

template ComponentMesh* GameObject::createComponent<ComponentMesh>();
template ComponentCamera* GameObject::createComponent<ComponentCamera>();
template ComponentDirectionalLight* GameObject::createComponent<ComponentDirectionalLight>();
template ComponentPointLight* GameObject::createComponent<ComponentPointLight>();
template ComponentSpotLight* GameObject::createComponent<ComponentSpotLight>();
template ComponentAnimation* GameObject::createComponent<ComponentAnimation>();
template ComponentCharacterMotion* GameObject::createComponent<ComponentCharacterMotion>();
template ComponentSimpleCharacterController* GameObject::createComponent<ComponentSimpleCharacterController>();
template ComponentRigidbody* GameObject::createComponent<ComponentRigidbody>();
template ComponentBounds* GameObject::createComponent<ComponentBounds>();
template ComponentDecal* GameObject::createComponent<ComponentDecal>();
template ComponentBillboard* GameObject::createComponent<ComponentBillboard>();
template ComponentParticleSystem* GameObject::createComponent<ComponentParticleSystem>();
template ComponentTrail* GameObject::createComponent<ComponentTrail>();
