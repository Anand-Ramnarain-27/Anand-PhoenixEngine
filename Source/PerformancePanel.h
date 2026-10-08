#pragma once
// Editor panel with the frame's GPU / CPU time, a per-pass breakdown and an FPS history graph.

#include "EditorPanel.h"
#include <cstdint>

class PerformancePanel : public EditorPanel {
public:
    explicit PerformancePanel(ModuleEditor* editor) : EditorPanel(editor){ open = false; }
    const char* getName() const override { return "Performance"; }

    void pushFPS(float fps){ m_fpsHistory[m_fpsIdx] = fps; m_fpsIdx = (m_fpsIdx + 1) % kHistory; }
    void setGpuMs(double ms){ m_gpuMs = ms; m_gpuReady = true; }
    void setMemory(uint64_t gpu, uint64_t ram){ m_gpuMem = gpu; m_ramMem = ram; }

protected:
    void drawContent() override;

private:
    void drawFrameSummary(float gpuMs, float cpuMs, float fps, float budgetPc);
    void drawPassBreakdown(float gpuMs, float scale, float refTotal);
    void drawFpsHistory();

    static constexpr int kHistory = 200;
    float m_fpsHistory[kHistory] = {};
    int m_fpsIdx = 0;
    double m_gpuMs = 0.0;
    bool m_gpuReady = false;
    uint64_t m_gpuMem = 0;
    uint64_t m_ramMem = 0;
};
