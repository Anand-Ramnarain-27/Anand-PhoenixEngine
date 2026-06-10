#pragma once
#include "Module.h"
#include "Frustum.h"

class FrustumDebugDraw;
class GameObject;

class ModuleCamera : public Module
{
public:
    enum class CullMode { None, Frustum };
    enum class CullSource { EditorCamera, GameCamera };

    // Gap 1 — hierarchical render culling algorithm. Linear = per-mesh frustum
    // plane test against every renderable (existing behaviour, kept as a
    // togglable fallback). Octree = query RenderOctree with the active game
    // camera frustum; objects not returned are auto-culled without per-object
    // plane tests.
    enum class CullAlgorithm { Linear, Octree };

    // Gap 2 — debug override for ComponentMesh LOD selection.
    // Auto = pick by screen-coverage thresholds; 0/1/2 force that LOD index.
    enum class ForceLOD { Auto, LOD0, LOD1, LOD2 };

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
    float getSpeedBoost() const { return speedBoostMultiplier; }

    Vector3 getForward() const;
    Vector3 getRight() const;
    Vector3 getUp() const;

    void focusOnTarget(const Vector3& target);
    static Matrix getPerspectiveProj(float aspect, float fov = XM_PIDIV4);

    const Frustum& getEditorFrustum() const { return m_editorFrustum; }
    const Frustum& getCullFrustum() const { return m_cullFrustum; }

    void setGameCameraFrustum(const Frustum& f) { m_gameFrustum = f; m_hasGameFrustum = true; }
    void clearGameCameraFrustum() { m_hasGameFrustum = false; }
    const Frustum& getGameFrustum() const { return m_gameFrustum; }
    bool hasGameFrustum() const { return m_hasGameFrustum; }

    bool isVisible(const Vector3& aabbMin, const Vector3& aabbMax) const;

    // --- Active Game Camera (Unity/Unreal "Game" view camera) ---------------
    // The active game camera determines the view/projection used for the final
    // game render output (GameViewPanel). It is fully independent from the
    // editor/scene-view fly camera (this ModuleCamera's own position/rotation),
    // mirroring Unity's Scene vs Game view separation.
    GameObject* getActiveCamera() const { return m_activeCameraGO; }
    void setActiveCamera(GameObject* go) { m_activeCameraGO = go; }

    void onEditorDebugPanel();
    void buildDebugLines(FrustumDebugDraw& dd) const;

    // Frustum-culling visibility stats, refreshed once per frame in
    // ModuleEditor::preRender() and displayed by onEditorDebugPanel().
    void setVisibilityStats(int visible, int total) { m_visibleCount = visible; m_totalCount = total; }
    int getVisibleCount() const { return m_visibleCount; }
    int getCulledCount() const { return m_totalCount - m_visibleCount; }
    int getTotalCount() const { return m_totalCount; }

    // ImGui toggle: "Show Frustum Culling Debug" — draws the active game
    // camera's frustum wireframe plus per-object AABBs (green = visible,
    // red = culled) in the scene viewport only. Never affects the game render.
    bool showFrustumCullingDebug = false;

    CullMode cullMode = CullMode::Frustum;
    CullSource cullSource = CullSource::EditorCamera;

    // Gap 1 — "Culling Mode: Linear | Octree" toggle (Debug -> Camera & Culling).
    CullAlgorithm cullAlgorithm = CullAlgorithm::Linear;
    // Stats from the most recent RenderOctree query (only meaningful when
    // cullAlgorithm == Octree); shown next to the visible/total counter.
    int octreeNodeCount = 0;
    int octreeLeafCount = 0;

    // Gap 2 — "Force LOD: Auto | 0 | 1 | 2" debug override (Debug -> Camera & Culling).
    ForceLOD forceLOD = ForceLOD::Auto;

    // Gap 3 — AI culling tunables (Debug -> Camera & Culling).
    // Off-screen AI further than this from the active game camera ticks at
    // 1/aiCullTickRate of the normal rate instead of every frame.
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
    struct Params
    {
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
