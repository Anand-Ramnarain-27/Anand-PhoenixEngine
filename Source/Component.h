#pragma once

#include "ModuleD3D12.h"
#include "ModuleCamera.h"
class GameObject;

class Component
{
public:
    explicit Component(GameObject* owner) : owner(owner) {}
    virtual ~Component() = default;

    virtual void update(float) {}
    virtual void render(ID3D12GraphicsCommandList*, const ModuleCamera&, const Matrix&) {}

protected:
    GameObject* owner;
};
