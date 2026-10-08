#pragma once
// The editor's free-fly / orbit camera, plus the frustum-culling and LOD debug settings the renderer reads.

#include "Module.h"
#include "Frustum.h"

class FrustumDebugDraw;
class GameObject;

/// Editor camera: right mouse + WASD/QE flies, Alt + left mouse orbits the origin, the wheel dollies, F refocuses,
/// Shift speeds everything up; a connected gamepad flies too. Also holds the game camera's frustum (set by the
/// active ComponentCamera) so culling can run from either camera, and the culling / LOD / AI-tick debug options.
class ModuleCamera : public Module {
public:
    enum class CullMode { None, Frustum };
    enum class CullSource { EditorCamera, GameCamera };

    enum class CullAlgorithm { Linear, Octree };

    enum class ForceLOD { Auto, LOD0, LOD1, LOD2 };

    bool init() override;
    void update() override;

    const Matrix& getView() const { return m_view; }
    const Quaternion& getRot() const { return m_rotation; }
    const Vector3& getPos() const { return m_position; }

    float getPolar() const { return m_params.polar; }
    float getAzimuthal() const { return m_params.azimuthal; }
    const Vector3& getTranslation() const { return m_params.translation; }

    void setPolar(float p){ m_params.polar = p; }
    void setAzimuthal(float a){ m_params.azimuthal = a; }
    void setTranslation(const Vector3& t){ m_params.translation = t; }

    void setSpeedBoost(float m){ m_speedBoostMultiplier = m; }
    float getSpeedBoost() const { return m_speedBoostMultiplier; }

    Vector3 getForward() const;
    Vector3 getRight() const;
    Vector3 getUp() const;

    /// Moves to 5 units from `target`, keeping the current direction, and looks at it.
    void focusOnTarget(const Vector3& target);
    /// Default editor projection (near 0.1, far 500).
    static Matrix getPerspectiveProj(float aspect, float fov = XM_PIDIV4);

    const Frustum& getEditorFrustum() const { return m_editorFrustum; }
    const Frustum& getCullFrustum() const { return m_cullFrustum; }

    /// The active game camera's frustum, which RuntimeCore culls against.
    void setGameCameraFrustum(const Frustum& f){ m_gameFrustum = f; m_hasGameFrustum = true; }
    void clearGameCameraFrustum(){ m_hasGameFrustum = false; }
    const Frustum& getGameFrustum() const { return m_gameFrustum; }
    bool hasGameFrustum() const { return m_hasGameFrustum; }

    /// Against the cull frustum (editor or game camera, per cullSource); always true with culling off.
    bool isVisible(const Vector3& aabbMin, const Vector3& aabbMax) const;

    GameObject* getActiveCamera() const { return m_activeCameraGO; }
    void setActiveCamera(GameObject* go){ m_activeCameraGO = go; }

    void onEditorDebugPanel();
    void buildDebugLines(FrustumDebugDraw& dd) const;

    void setVisibilityStats(int visible, int total){ m_visibleCount = visible; m_totalCount = total; }
    int getVisibleCount() const { return m_visibleCount; }
    int getCulledCount() const { return m_totalCount - m_visibleCount; }
    int getTotalCount() const { return m_totalCount; }

    bool showFrustumCullingDebug = false;

    CullMode cullMode = CullMode::Frustum;
    CullSource cullSource = CullSource::EditorCamera;

    CullAlgorithm cullAlgorithm = CullAlgorithm::Linear;
    int octreeNodeCount = 0;
    int octreeLeafCount = 0;

    ForceLOD forceLOD = ForceLOD::Auto;

    /// Passed to every script's IScript::shouldTickAI, which decides whether a distant or off-screen script skips
    /// its update this frame.
    float aiCullDistance = 50.0f;
    int aiCullTickRate = 10;

    bool debugDrawEditorFrustum = true;
    bool debugDrawCullFrustum = true;
    bool debugDrawCameraAxes = true;
    bool debugDrawForwardRay = true;

    float fovY = XM_PIDIV4;
    float nearZ = 0.1f;
    float farZ = 500.0f;
    float aspectRatio = 16.0f / 9.0f;

private:
    struct Params {
        float polar = 0.0f;
        float azimuthal = 0.0f;
        Vector3 translation = { 0.0f, 2.0f, 10.0f };
    };

    Params m_params;
    Quaternion m_rotation;
    Vector3 m_position;
    Matrix m_view = Matrix::Identity;

    int m_dragPosX = 0;
    int m_dragPosY = 0;
    int m_previousWheelValue = 0;
    bool m_prevFKeyState = false;

    float m_speedMultiplier = 1.0f;
    float m_speedBoostMultiplier = 5.0f;

    Frustum m_editorFrustum;
    Frustum m_gameFrustum;
    Frustum m_cullFrustum;
    bool m_hasGameFrustum = false;

    GameObject* m_activeCameraGO = nullptr;
    int m_visibleCount = 0;
    int m_totalCount = 0;

    void rebuildViewMatrix();
    void rebuildFrustum();
    void updateFlyMode(float dt, const Vector3& translate, const Vector2& rotateDelta);
    void updateOrbitMode(const Vector2& rotateDelta);
};
