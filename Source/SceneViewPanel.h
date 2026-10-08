#pragma once
// The Scene view: the editor camera's view with gizmos, toolbar, picking and asset drops.

#include "ViewportPanel.h"
#include "AssetBrowserPanel.h"
#include <ImGuizmo.h>

/// Renders with editor extras (debug draw, light proxies), draws the ImGuizmo transform gizmo for the
/// selection and the toolbar (gizmo mode, play controls, view options).
class SceneViewPanel : public ViewportPanel {
public:
    explicit SceneViewPanel(ModuleEditor* editor);
    const char* getName() const override { return "Viewport"; }
    void draw() override;

protected:
    bool buildCameraMatrices(uint32_t w, uint32_t h, Matrix& outView, Matrix& outProj) override;
    void onPostRender(ID3D12GraphicsCommandList* cmd, uint32_t w, uint32_t h) override;
    void onResized(uint32_t w, uint32_t h) override;
    void onImageDrawn() override;
    void onDrawOverlays() override;
    bool useEditorExtras() const override { return true; }
    const char* notReadyText() const override { return "Scene View not ready..."; }

private:
    void drawGizmoToolbar();
    void handleGizmoHotkeys();
    void drawTransformButtons(ImDrawList* dl, ImVec2 toolOrigin, float toolH, float btnSz);
    void drawPlayControls(ImVec2 toolOrigin, float cW, float btnSz);
    void drawViewOptions(ImVec2 toolOrigin, float cW, float btnSz);
    void drawGizmo();
    void drawOverlay();
    void drawPrefabExitButton();

    ImGuizmo::OPERATION m_gizmoOp = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_gizmoMode = ImGuizmo::LOCAL;
    bool m_useSnap = false;
    float m_snapT[3] = { 0.5f, 0.5f, 0.5f };
    float m_snapR = 15.0f;
    float m_snapS = 0.1f;
    bool m_fullscreen = false;
    ImGuiID m_savedDockId = 0;
};
