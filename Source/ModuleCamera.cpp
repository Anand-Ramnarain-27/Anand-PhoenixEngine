#include "Globals.h"
#include "ModuleCamera.h"
#include "FrustumDebugDraw.h"
#include "Application.h"
#include "Mouse.h"
#include "Keyboard.h"
#include "GamePad.h"
#include <imgui.h>
#include <algorithm>

static constexpr float kOrbitSensitivity = 0.005f;   // radians per mouse pixel
static constexpr float kPanSpeed = 1.0f;
static constexpr float kZoomSpeed = 1.0f;
static constexpr float kTriggerClimbSpeed = 0.25f;   // gamepad triggers move the camera up / down
// Pitch stops short of straight up / down, where the yaw axis would flip.
static constexpr float kMaxPitch = XM_PIDIV2 - 0.01f;
static constexpr float kMinOrbitRadius = 0.5f;
static constexpr float kFocusDistance = 5.0f;
static constexpr float kForwardRayLength = 20.0f;   // debug ray, capped by the far plane
static constexpr float kDefaultNear = 0.1f;
static constexpr float kDefaultFar = 500.0f;

bool ModuleCamera::init(){
    m_params = {};
    m_position = m_params.translation;
    m_rotation = Quaternion::Identity;
    focusOnTarget(Vector3::Zero);
    return true;
}

void ModuleCamera::update(){
    const Mouse::State& ms = Mouse::Get().GetState();
    const Keyboard::State& ks = Keyboard::Get().GetState();
    GamePad::State gps = GamePad::Get().GetState(0);

    const float dt = app->getElapsedMilis() * 0.001f;
    m_speedMultiplier = (ks.LeftShift || ks.RightShift) ? m_speedBoostMultiplier : 1.0f;

    const bool isOrbiting = (ks.LeftAlt || ks.RightAlt) && ms.leftButton;
    const bool isFlyMode = ms.rightButton && !isOrbiting;

    const Vector2 mouseDelta(float(m_dragPosX - ms.x), float(m_dragPosY - ms.y));
    const int wheelDelta = ms.scrollWheelValue - m_previousWheelValue;
    m_previousWheelValue = ms.scrollWheelValue;

    Vector3 translateLocal = Vector3::Zero;
    Vector2 rotateDelta = Vector2::Zero;

    if (isFlyMode || isOrbiting){
        rotateDelta.x = mouseDelta.x * kOrbitSensitivity * m_speedMultiplier;
        rotateDelta.y = mouseDelta.y * kOrbitSensitivity * m_speedMultiplier;
    }

    if (gps.IsConnected()){
        rotateDelta.x += -gps.thumbSticks.rightX * dt * m_speedMultiplier;
        rotateDelta.y += -gps.thumbSticks.rightY * dt * m_speedMultiplier;
        translateLocal.x += gps.thumbSticks.leftX * dt * m_speedMultiplier;
        translateLocal.z += -gps.thumbSticks.leftY * dt * m_speedMultiplier;
        if (gps.IsLeftTriggerPressed()) translateLocal.y += kTriggerClimbSpeed * dt * m_speedMultiplier;
        if (gps.IsRightTriggerPressed()) translateLocal.y -= kTriggerClimbSpeed * dt * m_speedMultiplier;
    }

    if (wheelDelta != 0) translateLocal.z -= float(wheelDelta) * kZoomSpeed * dt * m_speedMultiplier;

    if (isFlyMode){
        const float mv = kPanSpeed * dt * m_speedMultiplier;
        if (ks.W) translateLocal.z -= mv;
        if (ks.S) translateLocal.z += mv;
        if (ks.A) translateLocal.x -= mv;
        if (ks.D) translateLocal.x += mv;
        if (ks.Q) translateLocal.y += mv;
        if (ks.E) translateLocal.y -= mv;
    }

    if (ks.F && !m_prevFKeyState) focusOnTarget(Vector3::Zero);
    m_prevFKeyState = ks.F;

    if (isOrbiting) updateOrbitMode(rotateDelta);
    else updateFlyMode(dt, translateLocal, rotateDelta);

    m_dragPosX = ms.x;
    m_dragPosY = ms.y;

    rebuildFrustum();
}

void ModuleCamera::rebuildFrustum(){
    m_editorFrustum = Frustum::fromCamera(m_position, getForward(), getRight(), getUp(), fovY, aspectRatio, nearZ, farZ);
    m_cullFrustum = (cullSource == CullSource::GameCamera && m_hasGameFrustum) ? m_gameFrustum : m_editorFrustum;
}

bool ModuleCamera::isVisible(const Vector3& aabbMin, const Vector3& aabbMax) const{
    return cullMode == CullMode::None || m_cullFrustum.intersectsAABB(aabbMin, aabbMax);
}

void ModuleCamera::buildDebugLines(FrustumDebugDraw& dd) const{
    if (debugDrawEditorFrustum && m_editorFrustum.cornersValid) dd.addFrustum(m_editorFrustum, Vector3(1, 1, 1));

    if (debugDrawCullFrustum && m_cullFrustum.cornersValid){
        bool isGameCam = (cullSource == CullSource::GameCamera && m_hasGameFrustum);
        dd.addFrustum(m_cullFrustum, isGameCam ? Vector3(0, 1, 0) : Vector3(1, 1, 0));
    }

    if (debugDrawCameraAxes) dd.addAxes(m_position, getForward(), getRight(), getUp(), 0.5f);

    if (debugDrawForwardRay){
        Vector3 fwd = getForward();
        dd.addLine(m_position, m_position + fwd * nearZ, Vector3(0, 1, 1));
        dd.addLine(m_position + fwd * nearZ, m_position + fwd * std::min(farZ, kForwardRayLength), Vector3(0, 0.5f, 0.5f));
    }
}

void ModuleCamera::onEditorDebugPanel(){
    if (!ImGui::CollapsingHeader("Frustum Culling", ImGuiTreeNodeFlags_DefaultOpen)) return;

    int cm = (int)cullMode;
    ImGui::Text("Cull Mode"); ImGui::SameLine();
    if (ImGui::RadioButton("Off##cm", &cm, 0)) cullMode = CullMode::None;
    ImGui::SameLine();
    if (ImGui::RadioButton("Frustum##cm", &cm, 1)) cullMode = CullMode::Frustum;

    int cs = (int)cullSource;
    ImGui::Text("Cull From"); ImGui::SameLine();
    if (ImGui::RadioButton("Editor Cam##cs", &cs, 0)) cullSource = CullSource::EditorCamera;
    ImGui::SameLine();
    if (ImGui::RadioButton("Game Cam##cs", &cs, 1)) cullSource = CullSource::GameCamera;

    if (cullSource == CullSource::GameCamera && !m_hasGameFrustum) ImGui::TextColored(ImVec4(1, 0.4f, 0, 1), "  No game camera frustum set");

    ImGui::Separator();
    ImGui::Text("Debug Draw");
    ImGui::Checkbox("Editor Frustum (white)", &debugDrawEditorFrustum);
    ImGui::Checkbox("Cull Frustum (yellow/green)", &debugDrawCullFrustum);
    ImGui::Checkbox("Camera Axes", &debugDrawCameraAxes);
    ImGui::Checkbox("Forward Ray (cyan)", &debugDrawForwardRay);

    ImGui::Separator();
    ImGui::Checkbox("Show Frustum Culling Debug", &showFrustumCullingDebug);
    if (showFrustumCullingDebug && !m_hasGameFrustum)
        ImGui::TextColored(ImVec4(1, 0.4f, 0, 1), "  No active game camera frustum set");
    ImGui::Text("Visible: %d / Total: %d  |  Culled: %d", m_visibleCount, m_totalCount, getCulledCount());

    ImGui::Separator();
    ImGui::Text("Culling Mode"); ImGui::SameLine();
    int ca = (int)cullAlgorithm;
    if (ImGui::RadioButton("Linear##ca", &ca, 0)) cullAlgorithm = CullAlgorithm::Linear;
    ImGui::SameLine();
    if (ImGui::RadioButton("Octree##ca", &ca, 1)) cullAlgorithm = CullAlgorithm::Octree;
    if (cullAlgorithm == CullAlgorithm::Octree)
        ImGui::Text("Octree Nodes: %d  |  Leaves: %d", octreeNodeCount, octreeLeafCount);

    ImGui::Separator();
    ImGui::Text("Force LOD"); ImGui::SameLine();
    int fl = (int)forceLOD;
    if (ImGui::RadioButton("Auto##lod", &fl, 0)) forceLOD = ForceLOD::Auto;
    ImGui::SameLine();
    if (ImGui::RadioButton("0##lod", &fl, 1)) forceLOD = ForceLOD::LOD0;
    ImGui::SameLine();
    if (ImGui::RadioButton("1##lod", &fl, 2)) forceLOD = ForceLOD::LOD1;
    ImGui::SameLine();
    if (ImGui::RadioButton("2##lod", &fl, 3)) forceLOD = ForceLOD::LOD2;

    ImGui::Separator();
    ImGui::Text("AI Culling");
    ImGui::DragFloat("AI Cull Distance", &aiCullDistance, 1.0f, 0.0f, 1000.0f);
    ImGui::DragInt("AI Cull Tick Rate", &aiCullTickRate, 1, 1, 60);

    ImGui::Separator();
    ImGui::Text("Camera Parameters");
    float fovDeg = XMConvertToDegrees(fovY);
    if (ImGui::SliderFloat("FOV (Y)", &fovDeg, 10.f, 170.f)) fovY = XMConvertToRadians(fovDeg);
    ImGui::DragFloat("Near", &nearZ, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Far", &farZ, 1.0f, 10.0f, 5000.0f);
    ImGui::SliderFloat("Aspect Ratio", &aspectRatio, 0.5f, 4.0f);

    ImGui::Separator();
    Vector3 fwd = getForward();
    ImGui::Text("Position: %.2f  %.2f  %.2f", m_position.x, m_position.y, m_position.z);
    ImGui::Text("Forward:  %.2f  %.2f  %.2f", fwd.x, fwd.y, fwd.z);
}

void ModuleCamera::updateFlyMode(float, const Vector3& translateLocal, const Vector2& rotateDelta){
    m_params.polar += rotateDelta.x;
    m_params.azimuthal = std::clamp(m_params.azimuthal + rotateDelta.y, -kMaxPitch, kMaxPitch);
    m_rotation = Quaternion::CreateFromAxisAngle(Vector3::UnitX, m_params.azimuthal) * Quaternion::CreateFromAxisAngle(Vector3::UnitY, m_params.polar);
    m_params.translation += Vector3::Transform(translateLocal, m_rotation);
    m_position = m_params.translation;
    rebuildViewMatrix();
}

void ModuleCamera::updateOrbitMode(const Vector2& rotateDelta){
    m_params.polar += rotateDelta.x;
    m_params.azimuthal = std::clamp(m_params.azimuthal + rotateDelta.y, -kMaxPitch, kMaxPitch);
    const float r = std::max((m_position - Vector3::Zero).Length(), kMinOrbitRadius);
    m_position = { r * sinf(m_params.polar) * cosf(m_params.azimuthal), r * sinf(m_params.azimuthal), r * cosf(m_params.polar) * cosf(m_params.azimuthal) };
    m_params.translation = m_position;
    m_view = Matrix::CreateLookAt(m_position, Vector3::Zero, Vector3::UnitY);
    m_rotation = Quaternion::CreateFromRotationMatrix(Matrix(m_view).Invert());
}

void ModuleCamera::rebuildViewMatrix(){
    Quaternion inv;
    m_rotation.Inverse(inv);
    m_view = Matrix::CreateFromQuaternion(inv);
    m_view.Translation(Vector3::Transform(-m_position, inv));
}

void ModuleCamera::focusOnTarget(const Vector3& target){
    Vector3 dir = m_position - target;
    if (dir.LengthSquared() < 1e-6f) dir = Vector3(0.0f, 0.0f, kFocusDistance);
    dir.Normalize();
    m_params.translation = target + dir * kFocusDistance;
    m_position = m_params.translation;
    m_view = Matrix::CreateLookAt(m_position, target, Vector3::UnitY);
    m_rotation = Quaternion::CreateFromRotationMatrix(Matrix(m_view).Invert());
    m_params.polar = atan2f(dir.x, dir.z);
    m_params.azimuthal = asinf(std::clamp(-dir.y, -1.0f, 1.0f));
}

Matrix ModuleCamera::getPerspectiveProj(float aspect, float fov){ return Matrix::CreatePerspectiveFieldOfView(fov, aspect, kDefaultNear, kDefaultFar); }
Vector3 ModuleCamera::getForward() const { return Vector3::Transform(-Vector3::UnitZ, m_rotation); }
Vector3 ModuleCamera::getRight() const { return Vector3::Transform(Vector3::UnitX, m_rotation); }
Vector3 ModuleCamera::getUp() const { return Vector3::Transform(Vector3::UnitY, m_rotation); }
