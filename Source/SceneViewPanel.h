#pragma once
#include "EditorPanel.h"
#include "EditorViewport.h"
#include <ImGuizmo.h>

class SceneViewPanel : public EditorPanel
{
public:
    explicit SceneViewPanel(ModuleEditor* editor);

    void draw() override;
    const char* getName() const override { return "Scene View"; }

    void renderToTexture(ID3D12GraphicsCommandList* cmd);
    void handleResize();

    EditorViewport viewport;

private:
    void drawGizmoToolbar();
    void drawGizmo();
    void drawOverlay();

    ImGuizmo::OPERATION m_gizmoOp = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE      m_gizmoMode = ImGuizmo::LOCAL;
    bool                m_useSnap = false;
    float               m_snapT[3] = { 0.25f, 0.25f, 0.25f };
    float               m_snapR = 15.0f;
    float               m_snapS = 0.1f;
};