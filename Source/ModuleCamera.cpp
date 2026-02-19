#include "Globals.h"
#include "ModuleCamera.h"
#include "FrustumDebugDraw.h"
#include "Application.h"
#include "MathUtils.h"
#include "Mouse.h"
#include "Keyboard.h"
#include "GamePad.h"
#include <imgui.h>
#include <algorithm>

static constexpr float ORBIT_SENSITIVITY = 0.005f;
static constexpr float PAN_SPEED = 1.0f;
static constexpr float ZOOM_SPEED = 1.0f;

bool ModuleCamera::init()
{
    params = {};
    position = params.translation;
    rotation = Quaternion::Identity;
    focusOnTarget(Vector3::Zero);
    return true;
}

void ModuleCamera::update()
{
    const Mouse::State& ms = Mouse::Get().GetState();
    const Keyboard::State& ks = Keyboard::Get().GetState();
    GamePad::State         gps = GamePad::Get().GetState(0);

    const float dt = app->getElapsedMilis() * 0.001f;
    speedMultiplier = (ks.LeftShift || ks.RightShift) ? speedBoostMultiplier : 1.0f;

    const bool isOrbiting = (ks.LeftAlt || ks.RightAlt) && ms.leftButton;
    const bool isFlyMode = ms.rightButton && !isOrbiting;

    const Vector2 mouseDelta(float(dragPosX - ms.x), float(dragPosY - ms.y));
    const int     wheelDelta = ms.scrollWheelValue - previousWheelValue;
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

    if (ks.F && !prevFKeyState) focusOnTarget(Vector3::Zero);
    prevFKeyState = ks.F;

    if (isOrbiting) updateOrbitMode(rotateDelta);
    else            updateFlyMode(dt, translateLocal, rotateDelta);

    dragPosX = ms.x;
    dragPosY = ms.y;

    rebuildFrustum();
}

void ModuleCamera::rebuildFrustum()
{
    m_editorFrustum = Frustum::fromCamera(
        position, getForward(), getRight(), getUp(),
        fovY, aspectRatio, nearZ, farZ);

    if (cullSource == CullSource::GameCamera && m_hasGameFrustum)
        m_cullFrustum = m_gameFrustum;
    else
        m_cullFrustum = m_editorFrustum;
}

bool ModuleCamera::isVisible(const Vector3& aabbMin, const Vector3& aabbMax) const
{
    if (cullMode == CullMode::None) return true;
    return m_cullFrustum.intersectsAABB(aabbMin, aabbMax);
}

void ModuleCamera::buildDebugLines(FrustumDebugDraw& dd) const
{
    const Vector3 fwd = getForward();
    const Vector3 right = getRight();
    const Vector3 up = getUp();

    if (debugDrawEditorFrustum && m_editorFrustum.cornersValid)
        dd.addFrustum(m_editorFrustum, Vector3(1, 1, 1));

    if (debugDrawCullFrustum && m_cullFrustum.cornersValid)
    {
        bool isGameCam = (cullSource == CullSource::GameCamera && m_hasGameFrustum);
        dd.addFrustum(m_cullFrustum, isGameCam ? Vector3(0, 1, 0) : Vector3(1, 1, 0));
    }

    if (debugDrawCameraAxes)
        dd.addAxes(position, fwd, right, up, 0.5f);

    if (debugDrawForwardRay)
    {
        dd.addLine(position, position + fwd * nearZ, Vector3(0, 1, 1));
        dd.addLine(position + fwd * nearZ,
            position + fwd * std::min(farZ, 20.0f), Vector3(0, 0.5f, 0.5f)); 
    }
}

void ModuleCamera::onEditorDebugPanel()
{
    if (!ImGui::CollapsingHeader("Frustum Culling", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    int cm = (int)cullMode;
    ImGui::Text("Cull Mode");
    ImGui::SameLine();
    if (ImGui::RadioButton("Off##cm", &cm, 0)) cullMode = CullMode::None;
    ImGui::SameLine();
    if (ImGui::RadioButton("Frustum##cm", &cm, 1)) cullMode = CullMode::Frustum;

    int cs = (int)cullSource;
    ImGui::Text("Cull From");
    ImGui::SameLine();
    if (ImGui::RadioButton("Editor Cam##cs", &cs, 0)) cullSource = CullSource::EditorCamera;
    ImGui::SameLine();
    if (ImGui::RadioButton("Game Cam##cs", &cs, 1)) cullSource = CullSource::GameCamera;

    if (cullSource == CullSource::GameCamera && !m_hasGameFrustum)
        ImGui::TextColored(ImVec4(1, 0.4f, 0, 1), "  No game camera frustum set");

    ImGui::Separator();
    ImGui::Text("Debug Draw");
    ImGui::Checkbox("Editor Frustum (white)", &debugDrawEditorFrustum);
    ImGui::Checkbox("Cull Frustum (yellow/green)", &debugDrawCullFrustum);
    ImGui::Checkbox("Camera Axes", &debugDrawCameraAxes);
    ImGui::Checkbox("Forward Ray (cyan)", &debugDrawForwardRay);

    ImGui::Separator();
    ImGui::Text("Camera Parameters");

    float fovDeg = fovY * 57.2957795f;
    if (ImGui::SliderFloat("FOV (Y)", &fovDeg, 10.f, 170.f)) fovY = fovDeg * 0.0174532925f;
    ImGui::DragFloat("Near", &nearZ, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Far", &farZ, 1.0f, 10.0f, 5000.0f);
    ImGui::SliderFloat("Aspect Ratio", &aspectRatio, 0.5f, 4.0f);

    ImGui::Separator();
    ImGui::Text("Position: %.2f  %.2f  %.2f", position.x, position.y, position.z);
    Vector3 fwd = getForward();
    ImGui::Text("Forward:  %.2f  %.2f  %.2f", fwd.x, fwd.y, fwd.z);
    ImGui::Text("Lines buffered: (call buildDebugLines to count)");
}

void ModuleCamera::updateFlyMode(float, const Vector3& translateLocal, const Vector2& rotateDelta)
{
    params.polar += rotateDelta.x;
    params.azimuthal = std::clamp(params.azimuthal + rotateDelta.y, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);

    const Quaternion rotP = Quaternion::CreateFromAxisAngle(Vector3::UnitY, params.polar);
    const Quaternion rotA = Quaternion::CreateFromAxisAngle(Vector3::UnitX, params.azimuthal);
    rotation = rotA * rotP;

    params.translation += Vector3::Transform(translateLocal, rotation);
    position = params.translation;
    rebuildViewMatrix();
}

void ModuleCamera::updateOrbitMode(const Vector2& rotateDelta)
{
    params.polar += rotateDelta.x;
    params.azimuthal = std::clamp(params.azimuthal + rotateDelta.y, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);

    const float r = std::max((position - Vector3::Zero).Length(), 0.5f);
    position.x = r * sinf(params.polar) * cosf(params.azimuthal);
    position.y = r * sinf(params.azimuthal);
    position.z = r * cosf(params.polar) * cosf(params.azimuthal);
    params.translation = position;

    view = Matrix::CreateLookAt(position, Vector3::Zero, Vector3::UnitY);
    rotation = Quaternion::CreateFromRotationMatrix(Matrix(view).Invert());
}

void ModuleCamera::rebuildViewMatrix()
{
    Quaternion inv;
    rotation.Inverse(inv);
    view = Matrix::CreateFromQuaternion(inv);
    view.Translation(Vector3::Transform(-position, inv));
}

void ModuleCamera::focusOnTarget(const Vector3& target)
{
    Vector3 dir = position - target;
    if (dir.LengthSquared() < 1e-6f) dir = Vector3(0.0f, 0.0f, 5.0f);
    dir.Normalize();

    params.translation = target + dir * 5.0f;
    position = params.translation;

    view = Matrix::CreateLookAt(position, target, Vector3::UnitY);
    rotation = Quaternion::CreateFromRotationMatrix(Matrix(view).Invert());

    params.polar = atan2f(dir.x, dir.z);
    params.azimuthal = asinf(std::clamp(-dir.y, -1.0f, 1.0f));
}

Matrix  ModuleCamera::getPerspectiveProj(float aspect, float fov)
{
    return Matrix::CreatePerspectiveFieldOfView(fov, aspect, 0.1f, 500.0f);
}

Vector3 ModuleCamera::getForward() const { return Vector3::Transform(-Vector3::UnitZ, rotation); }
Vector3 ModuleCamera::getRight()   const { return Vector3::Transform(Vector3::UnitX, rotation); }
Vector3 ModuleCamera::getUp()      const { return Vector3::Transform(Vector3::UnitY, rotation); }