#pragma once
#include "Module.h"
#include "Frustum.h"

class FrustumDebugDraw;
class GameObject;

class ModuleCamera : public Module {
public:
    enum class CullMode { None, Frustum };
    enum class CullSource { EditorCamera, GameCamera };

    enum class CullAlgorithm { Linear, Octree };

    enum class ForceLOD { Auto, LOD0, LOD1, LOD2 };

    bool init() override;
    void update() override;

    void setEnable(bool flag){ enabled = flag; }
    bool getEnabled() const { return enabled; }

    const Matrix& getView() const { return view; }
    const Quaternion& getRot() const { return rotation; }
    const Vector3& getPos() const { return position; }

    float getPolar() const { return params.polar; }
    float getAzimuthal() const { return params.azimuthal; }
    const Vector3& getTranslation() const { return params.translation; }

    void setPolar(float p){ params.polar = p; }
    void setAzimuthal(float a){ params.azimuthal = a; }
    void setTranslation(const Vector3& t){ params.translation = t; }

    void setSpeedBoost(float m){ speedBoostMultiplier = m; }
    float getSpeedBoost() const { return speedBoostMultiplier; }

    Vector3 getForward() const;
    Vector3 getRight() const;
    Vector3 getUp() const;

    void focusOnTarget(const Vector3& target);
    static Matrix getPerspectiveProj(float aspect, float fov = XM_PIDIV4);

    const Frustum& getEditorFrustum() const { return m_editorFrustum; }
    const Frustum& getCullFrustum() const { return m_cullFrustum; }

    void setGameCameraFrustum(const Frustum& f){ m_gameFrustum = f; m_hasGameFrustum = true; }
    void clearGameCameraFrustum(){ m_hasGameFrustum = false; }
    const Frustum& getGameFrustum() const { return m_gameFrustum; }
    bool hasGameFrustum() const { return m_hasGameFrustum; }

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

    Params params;
    Quaternion rotation;
    Vector3 position;
    Matrix view = Matrix::Identity;

    int dragPosX = 0;
    int dragPosY = 0;
    int previousWheelValue = 0;
    bool prevFKeyState = false;
    bool enabled = true;

    float speedMultiplier = 1.0f;
    float speedBoostMultiplier = 5.0f;

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
