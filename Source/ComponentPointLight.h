#pragma once

#include "ComponentLight.h"

class ComponentPointLight : public ComponentLight
{
public:
    explicit ComponentPointLight(GameObject* owner);
    ~ComponentPointLight() override = default;

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;

    Type getType() const override { return Type::PointLight; }
    LightType getLightType() const override { return LightType::Point; }

    float getRadius() const { return m_radius; }
    void setRadius(float radius) { m_radius = radius; }

private:
    float m_radius = 5.0f;
};