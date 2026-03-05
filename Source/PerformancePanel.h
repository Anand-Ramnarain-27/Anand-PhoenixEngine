#pragma once
#include "EditorPanel.h"
#include "Application.h"

class PerformancePanel : public EditorPanel
{
public:
    explicit PerformancePanel(ModuleEditor* editor) : EditorPanel(editor) { open = false; }
    const char* getName() const override { return "Performance"; }

    inline void pushFPS(float fps) { m_fpsHistory[m_fpsIdx] = fps; m_fpsIdx = (m_fpsIdx + 1) % kHistory; }
    inline void setGpuMs(double ms) { m_gpuMs = ms; m_gpuReady = true; }
    inline void setMemory(uint64_t gpu, uint64_t ram) { m_gpuMem = gpu; m_ramMem = ram; }

protected:
    void drawContent() override
    {
        ImGui::Text("FPS:  %.1f", app->getFPS());
        ImGui::Text("CPU:  %.2f ms", app->getAvgElapsedMs());
        if (m_gpuReady) ImGui::Text("GPU:  %.2f ms", m_gpuMs);
        ImGui::Separator();
        ImGui::Text("VRAM: %llu MB", m_gpuMem);
        ImGui::Text("RAM:  %llu MB", m_ramMem);
        ImGui::Separator();

        float ordered[kHistory];
        float maxFPS = 60.0f;
        for (int i = 0; i < kHistory; ++i)
        {
            ordered[i] = m_fpsHistory[(m_fpsIdx + i) % kHistory];
            if (ordered[i] > maxFPS) maxFPS = ordered[i];
        }
        ImGui::PlotLines("##fps", ordered, kHistory, 0, nullptr, 0, maxFPS * 1.1f, ImVec2(-1, 80));
    }

private:
    static constexpr int kHistory = 200;
    float    m_fpsHistory[kHistory] = {};
    int      m_fpsIdx = 0;
    double   m_gpuMs = 0.0;
    bool     m_gpuReady = false;
    uint64_t m_gpuMem = 0;
    uint64_t m_ramMem = 0;
};