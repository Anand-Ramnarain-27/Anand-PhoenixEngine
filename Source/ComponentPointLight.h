#pragma once

#include "Component.h"
#include "ModuleD3D12.h"

class ComponentPointLight : public Component
{
public:
    explicit ComponentPointLight(GameObject* owner);
    ~ComponentPointLight() override = default;

    void onEditor()                         override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json)    override;
    Type getType() const override { return Type::PointLight; }

    Vector3 color = Vector3(1.f, 1.f, 1.f);
    float   intensity = 1.0f;
    float   radius = 5.0f;
    bool    enabled = true;
};