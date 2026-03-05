#pragma once
#include "EditorPanel.h"
#include "EditorViewport.h"
#include <d3d12.h>

class ViewportPanel : public EditorPanel
{
public:
    explicit ViewportPanel(ModuleEditor* editor) : EditorPanel(editor) {}

    void renderToTexture(ID3D12GraphicsCommandList* cmd);
    void handleResize();

    EditorViewport viewport;

protected:
    void drawContent() override;
    ImGuiWindowFlags windowFlags() const override { return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse; }
    bool noPadding() const override { return true; }

    virtual bool buildCameraMatrices(uint32_t w, uint32_t h, Matrix& outView, Matrix& outProj) = 0;
    virtual void onPostRender(ID3D12GraphicsCommandList* cmd, uint32_t w, uint32_t h) {}
    virtual void onResized(uint32_t w, uint32_t h) {}
    virtual void onImageDrawn() {}
    virtual void onDrawOverlays() {}
    virtual bool useEditorExtras() const = 0;
    virtual const char* notReadyText() const { return "Viewport not ready..."; }
};