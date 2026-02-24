#pragma once

#include "Component.h"
#include "ModuleD3D12.h"
#include "Frustum.h"

class ComponentCamera : public Component
{
public:
    explicit ComponentCamera(GameObject* owner);
    ~ComponentCamera() override = default;

    void update(float dt) override;

    void onEditor()      override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json)    override;
    Type getType() const override { return Type::Camera; }

    float getFOV()       const { return m_fov; }
    float getNearPlane() const { return m_nearPlane; }
    float getFarPlane()  const { return m_farPlane; }
    bool  isMainCamera() const { return m_isMainCamera; }
    const Vector4& getBackgroundColor() const { return m_backgroundColor; }
    const Frustum& getFrustum()         const { return m_frustum; }

    void setFOV(float v) { m_fov = v; }
    void setNearPlane(float v) { m_nearPlane = v; }
    void setFarPlane(float v) { m_farPlane = v; }
    void setMainCamera(bool v) { m_isMainCamera = v; }
    void setBackgroundColor(const Vector4& v) { m_backgroundColor = v; }

    Matrix getViewMatrix()                        const;
    Matrix getProjectionMatrix(float aspectRatio) const;

    void rebuildFrustum();

private:
    float   m_fov = 0.785398163f; 
    float   m_nearPlane = 0.1f;
    float   m_farPlane = 200.0f;
    bool    m_isMainCamera = false;
    Vector4 m_backgroundColor = { 0.2f, 0.3f, 0.4f, 1.0f };

    Frustum m_frustum;
};