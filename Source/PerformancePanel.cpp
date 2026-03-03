#include "Globals.h"
#include "PerformancePanel.h"
#include "ModuleEditor.h"
#include "Application.h"

void PerformancePanel::pushFPS(float fps)
{
    m_fpsHistory[m_fpsIdx] = fps;
    m_fpsIdx = (m_fpsIdx + 1) % kHistory;
}

void PerformancePanel::draw()
{
    ImGui::Begin("Performance", &open);
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
    ImGui::End();
}