#pragma once

#include "Module.h"

class ModuleCamera : public Module
{
    struct Params
    {
        float  polar;
        float  azimuthal;
        Vector3 translation;
    };

    Params params = { 0.0f, 0.0f , {0.0f , 2.0f , 10.0f } };
    Params tmpParams = { 0.0f, 0.0f , {0.0f , 0.0f , 0.0f } };
    int dragPosX = 0;
    int dragPosY = 0;

    Quaternion rotation;
    Vector3 position;
    Matrix view;
    bool enabled = true;

    float zoomSpeed = 0.5f;
    int previousWheelValue = 0;
    float zoomSensitivity = 0.1f;

    float speedMultiplier = 1.0f;
    float speedBoostMultiplier = 5.0f;

    bool prevFKeyState = false;
    void updateViewMatrix();
    void focusOnTarget(const Vector3& target);
public:

    bool init() override;
    void update() override;

    void setEnable(bool flag) { enabled = flag; }
    bool getEnabled() const { return enabled; }

    float getPolar() const { return params.polar; }
    float getAzimuthal() const { return params.azimuthal; }
    const Vector3& getTranslation() const { return params.translation; }

    void setPolar(float polar) { params.polar = polar; }
    void setAzimuthal(float azimuthal) { params.azimuthal = azimuthal; }
    void setTranslation(const Vector3& translation) { params.translation = translation; }

    const Matrix& getView() const { return view; }
    const Quaternion& getRot() const { return rotation; }
    const Vector3& getPos() const { return position; }

    static Matrix getPerspectiveProj(float aspect, float fov = XM_PIDIV4);

    void setSpeedBoost(float multiplier) { speedBoostMultiplier = multiplier; }
    float getSpeedBoost() const { return speedBoostMultiplier; }
};