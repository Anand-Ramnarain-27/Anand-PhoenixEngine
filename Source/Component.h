#pragma once

class GameObject;

class Component
{
public:
    explicit Component(GameObject* owner)
        : owner(owner) {
    }

    virtual ~Component() = default;

    virtual void render(ID3D12GraphicsCommandList*) {}
    virtual void update(float) {}
    virtual void onEditor() {}

protected:
    GameObject* owner = nullptr;
};
