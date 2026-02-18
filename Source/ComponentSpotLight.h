#pragma once
#include "Component.h"
#include "ModuleD3D12.h"

class ComponentSpotLight : public Component
{
public:
    explicit ComponentSpotLight(GameObject* owner);
    ~ComponentSpotLight() override = default;

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::SpotLight; }

    Vector3 direction = Vector3(0.f, -1.f, 0.f);
    Vector3 color = Vector3(1.f, 1.f, 1.f);
    float   intensity = 1.0f;
    float   innerAngle = 15.0f; 
    float   outerAngle = 30.0f; 
    float   radius = 10.0f;
    bool    enabled = true;
};