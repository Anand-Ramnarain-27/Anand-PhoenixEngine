#pragma once
#include "EditorPanel.h"
#include "EditorViewport.h"
#include <d3d12.h>

class GameViewPanel : public EditorPanel
{
public:
    explicit GameViewPanel(ModuleEditor* editor);

    void draw() override;
    const char* getName() const override { return "Game View"; }

    void renderToTexture(ID3D12GraphicsCommandList* cmd);
    void handleResize();

    EditorViewport viewport;

private:
    void drawPlaymodeOverlay();
};