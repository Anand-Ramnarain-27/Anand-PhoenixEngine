#pragma once
#include "Component.h"

class ComponentTransform : public Component
{
public:
    Vector3 position = { 0,0,0 };
    Vector3 scale = { 1,1,1 };
    Quaternion rotation = Quaternion::Identity;

    ComponentTransform(GameObject* owner);

    Matrix getLocalMatrix() const;
};
