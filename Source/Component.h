#pragma once

class GameObject;

class Component
{
public:
    explicit Component(GameObject* owner)
        : owner(owner) {
    }

    virtual ~Component() = default;

    virtual void update(float /*dt*/) {}
    virtual void onEditor() {}

protected:
    GameObject* owner = nullptr;
};
