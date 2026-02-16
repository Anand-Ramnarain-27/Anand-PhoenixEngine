#pragma once

#include "Component.h"
#include "ModuleD3D12.h"

class ComponentCamera : public Component
{
public:
    explicit ComponentCamera(GameObject* owner);
    ~ComponentCamera() override = default;

    void update(float deltaTime) override;
    void onEditor() override;

    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Camera; }

    float getFOV() const { return m_fov; }
    void setFOV(float fov) { m_fov = fov; }

    float getNearPlane() const { return m_nearPlane; }
    void setNearPlane(float nearPlane) { m_nearPlane = nearPlane; }

    float getFarPlane() const { return m_farPlane; }
    void setFarPlane(float farPlane) { m_farPlane = farPlane; }

    bool isMainCamera() const { return m_isMainCamera; }
    void setMainCamera(bool main) { m_isMainCamera = main; }

    Matrix getViewMatrix() const;
    Matrix getProjectionMatrix(float aspectRatio) const;

    const Vector4& getBackgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const Vector4& color) { m_backgroundColor = color; }

private:
    float m_fov;          
    float m_nearPlane;
    float m_farPlane;
    bool m_isMainCamera;
    Vector4 m_backgroundColor;
};