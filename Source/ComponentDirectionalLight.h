#pragma once

#include "ComponentLight.h"

class ComponentDirectionalLight : public ComponentLight
{
public:
    explicit ComponentDirectionalLight(GameObject* owner);
    ~ComponentDirectionalLight() override = default;

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;

    Type getType() const override { return Type::DirectionalLight; }
    LightType getLightType() const override { return LightType::Directional; }

    const Vector3& getDirection() const { return m_direction; }
    void setDirection(const Vector3& direction) { m_direction = direction; }

private:
    Vector3 m_direction = Vector3(-0.5f, -1.0f, -0.5f);
};