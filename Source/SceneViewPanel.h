#pragma once
#include "EditorPanel.h"
#include "EditorViewport.h"
#include "AssetBrowserPanel.h"   
#include <ImGuizmo.h>
#include <d3d12.h>

class SceneViewPanel : public EditorPanel
{
public:
    explicit SceneViewPanel(ModuleEditor* editor);
    const char* getName() const override { return "Scene View"; }

    void renderToTexture(ID3D12GraphicsCommandList* cmd);
    void handleResize();

    EditorViewport viewport;

protected:
    void drawContent() override;
    ImGuiWindowFlags windowFlags() const override
    {
        return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    }
    bool noPadding() const override { return true; }

private:
    void drawGizmoToolbar();
    void drawGizmo();
    void drawOverlay();

    ImGuizmo::OPERATION m_gizmoOp = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE      m_gizmoMode = ImGuizmo::LOCAL;
    bool  m_useSnap = false;
    float m_snapT[3] = { 0.25f, 0.25f, 0.25f };
    float m_snapR = 15.0f;
    float m_snapS = 0.1f;
};