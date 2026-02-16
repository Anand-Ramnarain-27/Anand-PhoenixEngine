#pragma once

#include <string>

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

    // Serialization interface - to be implemented by derived classes
    virtual void onSave(std::string& outJson) const {}
    virtual void onLoad(const std::string& json) {}

    // Component type identification for serialization
    enum class Type
    {
        Transform = 0,
        Mesh = 1,
        Camera = 2,
        // Add more component types as needed
    };

    virtual Type getType() const = 0;

protected:
    GameObject* owner = nullptr;
};