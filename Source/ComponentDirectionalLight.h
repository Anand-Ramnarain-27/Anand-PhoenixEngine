#pragma once

#include "Component.h"
#include "ModuleD3D12.h"

class ComponentDirectionalLight : public Component
{
public:
    explicit ComponentDirectionalLight(GameObject* owner);
    ~ComponentDirectionalLight() override = default;

    void onEditor()                         override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json)    override;
    Type getType() const override { return Type::DirectionalLight; }

    Vector3 direction = Vector3(-0.5f, -1.0f, -0.5f);
    Vector3 color = Vector3(1.0f, 1.0f, 1.0f);
    float   intensity = 1.0f;
    bool    enabled = true;
};