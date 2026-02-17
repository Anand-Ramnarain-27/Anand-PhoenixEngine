#pragma once

#include "ComponentLight.h"

class ComponentSpotLight : public ComponentLight
{
public:
    explicit ComponentSpotLight(GameObject* owner);
    ~ComponentSpotLight() override = default;

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;

    Type getType() const override { return Type::SpotLight; }
    LightType getLightType() const override { return LightType::Spot; }

    const Vector3& getDirection() const { return m_direction; }
    void setDirection(const Vector3& direction) { m_direction = direction; }

    float getInnerAngle() const { return m_innerAngle; }
    void setInnerAngle(float angle) { m_innerAngle = angle; }

    float getOuterAngle() const { return m_outerAngle; }
    void setOuterAngle(float angle) { m_outerAngle = angle; }

    float getRadius() const { return m_radius; }
    void setRadius(float radius) { m_radius = radius; }

private:
    Vector3 m_direction = Vector3(0.0f, -1.0f, 0.0f);
    float m_innerAngle = 15.0f;  // degrees
    float m_outerAngle = 30.0f;  // degrees
    float m_radius = 10.0f;
};