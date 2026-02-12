#pragma once

#include "UUID64.h"

class GameObject;

class Component
{
public:
    explicit Component(GameObject* owner)
        : owner(owner)
        , uuid(UUID64::Generate())  // Each component also gets a UUID
    {
    }

    virtual ~Component() = default;

    virtual void render(ID3D12GraphicsCommandList*) {}
    virtual void update(float) {}
    virtual void onEditor() {}

    // UUID for component-to-component references
    UUID64 getUUID() const { return uuid; }

    GameObject* getOwner() const { return owner; }

protected:
    GameObject* owner = nullptr;
    UUID64 uuid;
};
