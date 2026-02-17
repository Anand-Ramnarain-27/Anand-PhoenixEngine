#pragma once

#include "Module.h"

class ModuleCamera : public Module
{
public:
    bool init() override;
    void update() override;

    void setEnable(bool flag) { enabled = flag; }
    bool getEnabled() const { return enabled; }

    const Matrix& getView() const { return view; }
    const Quaternion& getRot() const { return rotation; }
    const Vector3& getPos() const { return position; }

    float getPolar() const { return params.polar; }
    float getAzimuthal() const { return params.azimuthal; }
    const Vector3& getTranslation() const { return params.translation; }

    void setPolar(float p) { params.polar = p; }
    void setAzimuthal(float a) { params.azimuthal = a; }
    void setTranslation(const Vector3& t) { params.translation = t; }

    void setSpeedBoost(float m) { speedBoostMultiplier = m; }
    float getSpeedBoost()  const { return speedBoostMultiplier; }

    Vector3 getForward() const;
    Vector3 getRight()   const;
    Vector3 getUp()      const;

    void focusOnTarget(const Vector3& target);

    static Matrix getPerspectiveProj(float aspect, float fov = XM_PIDIV4);

private:
    struct Params
    {
        float   polar = 0.0f;
        float   azimuthal = 0.0f;
        Vector3 translation = { 0.0f, 2.0f, 10.0f };
    };

    Params    params;
    Quaternion rotation;
    Vector3   position;
    Matrix    view = Matrix::Identity;

    int   dragPosX = 0;
    int   dragPosY = 0;
    int   previousWheelValue = 0;
    bool  prevFKeyState = false;

    bool  enabled = true;
    float zoomSpeed = 0.015f;  
    float panSpeed = 0.45f;    
    float orbitSensitivity = 0.005f;  
    float speedMultiplier = 1.0f;
    float speedBoostMultiplier = 5.0f;

    void rebuildViewMatrix();

    void updateFlyMode(float dt,
        const Vector3& translate,
        const Vector2& rotateDelta);

    void updateOrbitMode(const Vector2& rotateDelta);
};