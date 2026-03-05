#pragma once
#include "ViewportPanel.h"
#include "AssetBrowserPanel.h"
#include <ImGuizmo.h>

class SceneViewPanel : public ViewportPanel
{
public:
    explicit SceneViewPanel(ModuleEditor* editor);
    const char* getName() const override { return "Scene View"; }

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
    void drawGizmo();
    void drawOverlay();

    ImGuizmo::OPERATION m_gizmoOp = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE      m_gizmoMode = ImGuizmo::LOCAL;
    bool  m_useSnap = false;
    float m_snapT[3] = { 0.5f, 0.5f, 0.5f };
    float m_snapR = 15.0f;
    float m_snapS = 0.1f;
};