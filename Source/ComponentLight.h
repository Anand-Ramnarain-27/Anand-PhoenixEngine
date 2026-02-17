#pragma once

#include "Component.h"
#include "ModuleD3D12.h"

class ComponentLight : public Component
{
public:
    explicit ComponentLight(GameObject* owner);
    ~ComponentLight() override = default;

    enum class LightType
    {
        Directional,
        Point,
        Spot
    };

    virtual LightType getLightType() const = 0;

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    const Vector3& getColor() const { return m_color; }
    void setColor(const Vector3& color) { m_color = color; }

    float getIntensity() const { return m_intensity; }
    void setIntensity(float intensity) { m_intensity = intensity; }

protected:
    bool m_enabled = true;
    Vector3 m_color = Vector3(1.0f, 1.0f, 1.0f);
    float m_intensity = 1.0f;
};