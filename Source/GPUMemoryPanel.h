#pragma once
// Editor panel showing GPU memory: VRAM budget, descriptor heap usage, the upload ring and the static buffer pool.

#include "EditorPanel.h"

class GPUMemoryPanel : public EditorPanel {
public:
    explicit GPUMemoryPanel(ModuleEditor* editor) : EditorPanel(editor){}
    const char* getName() const override { return "GPU Memory"; }

protected:
    void drawContent() override;

private:
    struct UsageRow { const char* name; float used; float total; ImU32 color; };

    void drawBar(const char* tipId, float used, float total, ImU32 color);
    void sectionTitle(const char* text);
    void drawVramHeader();
    /// One row per entry: name, usage bar and "used/total". Column ids are "##<colPrefix>n", "b" and "v".
    void drawUsageTable(const char* tableId, const char* colPrefix, const UsageRow* rows, int count);
    void drawDescriptorHeaps();
    void drawRingBuffer();
    void drawBufferPools();
};
