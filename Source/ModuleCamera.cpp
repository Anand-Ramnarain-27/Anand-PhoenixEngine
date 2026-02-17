#include "Globals.h"
#include "ModuleCamera.h"
#include "Application.h"
#include "MathUtils.h"

#include "Mouse.h"
#include "Keyboard.h"
#include "GamePad.h"

#include <algorithm>

static constexpr float FAR_PLANE = 500.0f;
static constexpr float NEAR_PLANE = 0.1f;

static constexpr float ROTATION_SPEED_DEG = 25.0f;  
static constexpr float ORBIT_SENSITIVITY = 0.005f;  
static constexpr float PAN_SPEED = 0.45f; 
static constexpr float ZOOM_SPEED = 1.0f;

bool ModuleCamera::init()
{
    params.polar = 0.0f;
    params.azimuthal = 0.0f;
    params.translation = Vector3(0.0f, 2.0f, 10.0f);

    position = params.translation;
    rotation = Quaternion::Identity;

    focusOnTarget(Vector3::Zero);   

    return true;
}

void ModuleCamera::update()
{
    Mouse& mouse = Mouse::Get();
    Keyboard& keyboard = Keyboard::Get();
    GamePad& pad = GamePad::Get();

    const Mouse::State& ms = mouse.GetState();
    const Keyboard::State& ks = keyboard.GetState();
    GamePad::State         gps = pad.GetState(0);

    const float dt = app->getElapsedMilis() * 0.001f;

    speedMultiplier = (ks.LeftShift || ks.RightShift) ? speedBoostMultiplier : 1.0f;

    const bool isAltDown = ks.LeftAlt || ks.RightAlt;
    const bool isOrbiting = isAltDown && ms.leftButton;
    const bool isFlyMode = ms.rightButton && !isOrbiting;

    Vector2 mouseDelta = Vector2(
        float(dragPosX - ms.x),
        float(dragPosY - ms.y));

    const int wheelDelta = ms.scrollWheelValue - previousWheelValue;
    previousWheelValue = ms.scrollWheelValue;

    Vector3 translateLocal = Vector3::Zero;
    Vector2 rotateDelta = Vector2::Zero;  

    if (isFlyMode || isOrbiting)
    {
        rotateDelta.x = mouseDelta.x * ORBIT_SENSITIVITY * speedMultiplier;
        rotateDelta.y = mouseDelta.y * ORBIT_SENSITIVITY * speedMultiplier;
    }

    if (gps.IsConnected())
    {
        rotateDelta.x += -gps.thumbSticks.rightX * dt * speedMultiplier;
        rotateDelta.y += -gps.thumbSticks.rightY * dt * speedMultiplier;
        translateLocal.x += gps.thumbSticks.leftX * dt * speedMultiplier;
        translateLocal.z += -gps.thumbSticks.leftY * dt * speedMultiplier;
        if (gps.IsLeftTriggerPressed())  translateLocal.y += 0.25f * dt * speedMultiplier;
        if (gps.IsRightTriggerPressed()) translateLocal.y -= 0.25f * dt * speedMultiplier;
    }

    if (wheelDelta != 0)
        translateLocal.z -= float(wheelDelta) * ZOOM_SPEED * dt * speedMultiplier;

    if (isFlyMode)
    {
        const float mv = PAN_SPEED * dt * speedMultiplier;
        if (ks.W) translateLocal.z -= mv;
        if (ks.S) translateLocal.z += mv;
        if (ks.A) translateLocal.x -= mv;
        if (ks.D) translateLocal.x += mv;
        if (ks.Q) translateLocal.y += mv;
        if (ks.E) translateLocal.y -= mv;
    }

    if (ks.F && !prevFKeyState)
        focusOnTarget(Vector3::Zero);
    prevFKeyState = ks.F;

    if (isOrbiting)
        updateOrbitMode(rotateDelta);
    else
        updateFlyMode(dt, translateLocal, rotateDelta);

    dragPosX = ms.x;
    dragPosY = ms.y;
}

void ModuleCamera::updateFlyMode(float, const Vector3& translateLocal, const Vector2& rotateDelta)
{
    params.polar += rotateDelta.x;
    params.azimuthal += rotateDelta.y;

    params.azimuthal = std::clamp(params.azimuthal, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);

    const Quaternion rotPolar = Quaternion::CreateFromAxisAngle(Vector3::UnitY, params.polar);
    const Quaternion rotAzimuthal = Quaternion::CreateFromAxisAngle(Vector3::UnitX, params.azimuthal);
    rotation = rotAzimuthal * rotPolar;

    const Vector3 worldDir = Vector3::Transform(translateLocal, rotation);
    params.translation += worldDir;
    position = params.translation;

    rebuildViewMatrix();
}

void ModuleCamera::updateOrbitMode(const Vector2& rotateDelta)
{
    params.polar += rotateDelta.x;
    params.azimuthal += rotateDelta.y;
    params.azimuthal = std::clamp(params.azimuthal, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);

    const float radius = (position - Vector3::Zero).Length();
    const float safeR = (radius < 0.5f) ? 5.0f : radius;

    position.x = safeR * sinf(params.polar) * cosf(params.azimuthal);
    position.y = safeR * sinf(params.azimuthal);
    position.z = safeR * cosf(params.polar) * cosf(params.azimuthal);

    params.translation = position;

    view = Matrix::CreateLookAt(position, Vector3::Zero, Vector3::UnitY);
    rotation = Quaternion::CreateFromRotationMatrix(Matrix(view).Invert());
}

void ModuleCamera::rebuildViewMatrix()
{
    Quaternion invRot;
    rotation.Inverse(invRot);

    view = Matrix::CreateFromQuaternion(invRot);
    view.Translation(Vector3::Transform(-position, invRot));
}

void ModuleCamera::focusOnTarget(const Vector3& target)
{
    Vector3 dir = position - target;
    if (dir.LengthSquared() < 1e-6f)
        dir = Vector3(0.0f, 0.0f, 5.0f);
    dir.Normalize();

    params.translation = target + dir * 5.0f;
    position = params.translation;

    view = Matrix::CreateLookAt(position, target, Vector3::UnitY);
    rotation = Quaternion::CreateFromRotationMatrix(Matrix(view).Invert());

    params.polar = atan2f(dir.x, dir.z);
    params.azimuthal = asinf(std::clamp(-dir.y, -1.0f, 1.0f));
}

Matrix ModuleCamera::getPerspectiveProj(float aspect, float fov)
{
    return Matrix::CreatePerspectiveFieldOfView(fov, aspect, NEAR_PLANE, FAR_PLANE);
}

Vector3 ModuleCamera::getForward() const
{
    return Vector3::Transform(-Vector3::UnitZ, rotation);
}

Vector3 ModuleCamera::getRight() const
{
    return Vector3::Transform(Vector3::UnitX, rotation);
}

Vector3 ModuleCamera::getUp() const
{
    return Vector3::Transform(Vector3::UnitY, rotation);
}