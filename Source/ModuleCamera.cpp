#include "Globals.h"
#include "MathUtils.h"
#include <algorithm>

#include "ModuleCamera.h"

#include "Application.h"

#include "Mouse.h"
#include "Keyboard.h"
#include "GamePad.h"

#define FAR_PLANE 200.0f
#define NEAR_PLANE 0.1f

namespace
{
    constexpr float getRotationSpeed() { return 25.0f; }
    constexpr float getTranslationSpeed() { return 2.5f; }
}

bool ModuleCamera::init()
{
    position = Vector3(0.0f, 0.0f, 10.0f);
    rotation = Quaternion::CreateFromAxisAngle(Vector3(0.0f, 1.0f, 0.0f), XMConvertToRadians(0.0f));

    Quaternion invRot;
    rotation.Inverse(invRot);

    view = Matrix::CreateFromQuaternion(invRot);
    view.Translation(-position);

    view = Matrix::CreateLookAt(Vector3(0.0f, 0.0f, 10.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));

    return true;
}

void ModuleCamera::update()
{
    Mouse& mouse = Mouse::Get();
    const Mouse::State& mouseState = mouse.GetState();

    if (enabled)
    {
        Keyboard& keyboard = Keyboard::Get();
        GamePad& pad = GamePad::Get();

        const Keyboard::State& keyState = keyboard.GetState();
        GamePad::State padState = pad.GetState(0);

        float elapsedSec = app->getElapsedMilis() * 0.001f;

        speedMultiplier = 1.0f;
        if (keyState.LeftShift || keyState.RightShift)
        {
            speedMultiplier = speedBoostMultiplier;
        }

        Vector3 translate = Vector3::Zero;
        Vector2 rotate = Vector2::Zero;

        bool isAltPressed = keyState.LeftAlt || keyState.RightAlt;
        bool isOrbiting = isAltPressed && mouseState.leftButton;

        if (padState.IsConnected())
        {
            rotate.x = -padState.thumbSticks.rightX * elapsedSec * speedMultiplier;
            rotate.y = -padState.thumbSticks.rightY * elapsedSec * speedMultiplier;

            translate.x = padState.thumbSticks.leftX * elapsedSec * speedMultiplier;
            translate.z = -padState.thumbSticks.leftY * elapsedSec * speedMultiplier;

            if (padState.IsLeftTriggerPressed())
            {
                translate.y = 0.25f * elapsedSec * speedMultiplier;
            }
            else if (padState.IsRightTriggerPressed())
            {
                translate.y -= 0.25f * elapsedSec * speedMultiplier;
            }
        }

        if (mouseState.rightButton || isOrbiting)
        {
            rotate.x = float(dragPosX - mouseState.x) * 0.005f * speedMultiplier;
            rotate.y = float(dragPosY - mouseState.y) * 0.005f * speedMultiplier;
        }

        int wheelDelta = mouseState.scrollWheelValue - previousWheelValue;
        if (wheelDelta != 0)
        {
            translate.z += float(wheelDelta) * zoomSpeed * elapsedSec * speedMultiplier;
        }

        previousWheelValue = mouseState.scrollWheelValue;

        if (keyState.F && !prevFKeyState)
        {
            focusOnTarget(Vector3::Zero);
        }
        prevFKeyState = keyState.F;

        float moveSpeed = 0.45f * elapsedSec * speedMultiplier;
        if (keyState.W) translate.z -= moveSpeed;
        if (keyState.S) translate.z += moveSpeed;
        if (keyState.A) translate.x -= moveSpeed;
        if (keyState.D) translate.x += moveSpeed;
        if (keyState.Q) translate.y += moveSpeed;
        if (keyState.E) translate.y -= moveSpeed;

        if (isOrbiting)
        {
            params.polar += XMConvertToRadians(getRotationSpeed() * rotate.x);
            params.azimuthal += XMConvertToRadians(getRotationSpeed() * rotate.y);

            params.azimuthal = std::clamp(params.azimuthal, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);

            float radius = 5.0f; 
            position.x = radius * sin(params.polar) * cos(params.azimuthal);
            position.y = radius * sin(params.azimuthal);
            position.z = radius * cos(params.polar) * cos(params.azimuthal);

            position += Vector3::Zero;

            params.translation = position;

            view = Matrix::CreateLookAt(position, Vector3::Zero, Vector3::UnitY);
            rotation = Quaternion::CreateFromRotationMatrix(view.Invert());
        }

        Vector3 localDir = Vector3::Transform(translate, rotation);
        params.translation += localDir * getTranslationSpeed();
        params.polar += XMConvertToRadians(getRotationSpeed() * rotate.x);
        params.azimuthal += XMConvertToRadians(getRotationSpeed() * rotate.y);

        Quaternion rotation_polar = Quaternion::CreateFromAxisAngle(Vector3(0.0f, 1.0f, 0.0f), params.polar);
        Quaternion rotation_azimuthal = Quaternion::CreateFromAxisAngle(Vector3(1.0f, 0.0f, 0.0f), params.azimuthal);
        rotation = rotation_azimuthal * rotation_polar;
        position = params.translation;

        Quaternion invRot;
        rotation.Inverse(invRot);

        view = Matrix::CreateFromQuaternion(invRot);
        view.Translation(Vector3::Transform(-position, invRot));

    }

    dragPosX = mouseState.x;
    dragPosY = mouseState.y;
}

Matrix ModuleCamera::getPerspectiveProj(float aspect, float fov)
{
    return Matrix::CreatePerspectiveFieldOfView(fov, aspect, NEAR_PLANE, FAR_PLANE);
}

void ModuleCamera::getFrustumPlanes(Vector4 planes[6], float aspect, bool normalize) const
{
    Matrix proj = getPerspectiveProj(aspect);
    Matrix viewProjection = view * proj;

    getPlanes(planes, viewProjection, normalize);
}

BoundingFrustum ModuleCamera::getFrustum(float aspect) const
{
    Matrix proj = getPerspectiveProj(aspect);

    BoundingFrustum frustum;
    BoundingFrustum::CreateFromMatrix(frustum, proj, true);

    frustum.Origin = position;
    frustum.Orientation = rotation;

    return frustum;
}

void ModuleCamera::focusOnTarget(const Vector3& target)
{
    Vector3 dir = position - target;
    if (dir.Length() < 0.001f) dir = Vector3(0, 0, 5.0f);
    dir.Normalize();

    params.translation = target + dir * 5.0f;
    position = params.translation;

    view = Matrix::CreateLookAt(position, target, Vector3::UnitY);
    rotation = Quaternion::CreateFromRotationMatrix(view.Invert());
}

