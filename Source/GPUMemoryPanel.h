#pragma once
#include "EditorPanel.h"

class GPUMemoryPanel : public EditorPanel {
public:
    explicit GPUMemoryPanel(ModuleEditor* editor) : EditorPanel(editor) {}
    const char* getName() const override { return "GPU Memory"; }

protected:
    void drawContent() override;

private:
    static void drawHeapBar(const char* label, float used, float capacity,
                            const ImVec4& color, const char* unitSuffix = " desc");
    static void drawDonut(ImVec2 center, float radius, float fraction,
                          const ImVec4& color, const char* pctText, const char* subText);
};
