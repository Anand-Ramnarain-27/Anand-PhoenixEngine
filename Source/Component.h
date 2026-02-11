#pragma once

#include "UID.h"
class GameObject;

class Component
{
public:
    explicit Component(GameObject* owner)
        : owner(owner),
        uid(GenerateUID()) {
    }

    virtual ~Component() = default;

    virtual void render(ID3D12GraphicsCommandList*) {}
    virtual void update(float) {}
    virtual void onEditor() {}

    UID GetUID() const { return uid; }
protected:
    GameObject* owner = nullptr;
    UID uid;
};
