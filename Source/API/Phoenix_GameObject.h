#pragma once
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentRigidbody.h"
#include "ComponentAnimation.h"
#include "ComponentParticleSystem.h"
#include "ComponentScript.h"

// Null-safe one-liner helpers so scripts don't need to call go->getComponent<T>() directly.

namespace Phoenix {

inline ComponentTransform*     GetTransform (GameObject* go){ return go ? go->getTransform() : nullptr; }
inline ComponentRigidbody*     GetRigidbody (GameObject* go){ return go ? go->getComponent<ComponentRigidbody>() : nullptr; }
inline ComponentAnimation*     GetAnimation (GameObject* go){ return go ? go->getComponent<ComponentAnimation>() : nullptr; }
inline ComponentParticleSystem* GetParticles(GameObject* go){ return go ? go->getComponent<ComponentParticleSystem>() : nullptr; }
inline ComponentScript*        GetScript    (GameObject* go){ return go ? go->getComponent<ComponentScript>() : nullptr; }

inline void        SetActive(GameObject* go, bool active){ if (go) go->setActive(active); }
inline bool        IsActive (GameObject* go)             { return go && go->isActive(); }
inline const char* GetName  (GameObject* go)             { return go ? go->getName().c_str() : ""; }

// Shorthand transform accessors
inline Vec3& Position(GameObject* go){ return go->getTransform()->position; }
inline Vec3& Scale   (GameObject* go){ return go->getTransform()->scale; }
inline Quat& Rotation(GameObject* go){ return go->getTransform()->rotation; }

} // namespace Phoenix
