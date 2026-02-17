#pragma once

#include <string>
struct ID3D12GraphicsCommandList;

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

    virtual void onSave(std::string& outJson) const {}
    virtual void onLoad(const std::string& json) {}

    enum class Type
    {
        Transform = 0,
        Mesh = 1,
        Camera = 2,
    };

    virtual Type getType() const = 0;

protected:
    GameObject* owner = nullptr;
};