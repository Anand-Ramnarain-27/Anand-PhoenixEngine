#pragma once
#include "ViewportPanel.h"

class GameViewPanel : public ViewportPanel {
public:
    explicit GameViewPanel(ModuleEditor* editor);
    void draw() override;
    const char* getName() const override { return "Game View"; }

protected:
    bool buildCameraMatrices(uint32_t w, uint32_t h, Matrix& outView, Matrix& outProj) override;
    void onDrawOverlays() override;
    bool useEditorExtras() const override { return false; }
    const char* notReadyText() const override { return "Game View not ready..."; }

private:
    void drawPlaymodeOverlay();
};