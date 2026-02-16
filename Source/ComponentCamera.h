#pragma once

#include "Component.h"
#include "ModuleD3D12.h"  // For Vector4, Matrix types

// ComponentCamera - A camera component that can be attached to GameObjects
// This allows multiple cameras in the scene that can be serialized
class ComponentCamera : public Component
{
public:
    explicit ComponentCamera(GameObject* owner);
    ~ComponentCamera() override = default;

    void update(float deltaTime) override;
    void onEditor() override;

    // Serialization
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Camera; }

    // Camera settings
    float getFOV() const { return m_fov; }
    void setFOV(float fov) { m_fov = fov; }

    float getNearPlane() const { return m_nearPlane; }
    void setNearPlane(float nearPlane) { m_nearPlane = nearPlane; }

    float getFarPlane() const { return m_farPlane; }
    void setFarPlane(float farPlane) { m_farPlane = farPlane; }

    bool isMainCamera() const { return m_isMainCamera; }
    void setMainCamera(bool main) { m_isMainCamera = main; }

    // Get camera matrices based on this component's transform
    Matrix getViewMatrix() const;
    Matrix getProjectionMatrix(float aspectRatio) const;

    // Background color
    const Vector4& getBackgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const Vector4& color) { m_backgroundColor = color; }

private:
    float m_fov;                // Field of view in radians
    float m_nearPlane;
    float m_farPlane;
    bool m_isMainCamera;
    Vector4 m_backgroundColor;
};