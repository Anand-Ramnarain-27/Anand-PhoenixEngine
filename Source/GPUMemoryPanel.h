#pragma once
#include "EditorPanel.h"

class GPUMemoryPanel : public EditorPanel {
public:
    explicit GPUMemoryPanel(ModuleEditor* editor) : EditorPanel(editor){}
    const char* getName() const override { return "GPU Memory"; }

protected:
    void drawContent() override;

private:
    void drawBar(const char* tipId, float used, float total, ImU32 color);
};
