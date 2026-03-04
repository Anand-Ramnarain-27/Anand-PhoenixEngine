#pragma once
#include "EditorPanel.h"

class PerformancePanel : public EditorPanel
{
public:
    explicit PerformancePanel(ModuleEditor* editor) : EditorPanel(editor) { open = false; }
    void draw() override;
    const char* getName() const override { return "Performance"; }

    void pushFPS(float fps);
    void setGpuMs(double ms) { m_gpuMs = ms; m_gpuReady = true; }
    void setMemory(uint64_t gpuMB, uint64_t ramMB) { m_gpuMem = gpuMB; m_ramMem = ramMB; }

private:
    static constexpr int kHistory = 200;
    float    m_fpsHistory[kHistory] = {};
    int      m_fpsIdx = 0;
    double   m_gpuMs = 0.0;
    bool     m_gpuReady = false;
    uint64_t m_gpuMem = 0;
    uint64_t m_ramMem = 0;
};