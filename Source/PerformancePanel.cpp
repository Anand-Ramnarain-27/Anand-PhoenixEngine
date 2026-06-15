#include "Globals.h"
#include "PerformancePanel.h"
#include "EditorColors.h"
#include "ImGuiPass.h"
#include "Application.h"
#include "ModuleEditor.h"

void PerformancePanel::drawContent(){
    struct PassInfo { const char* name; float refMs; ImU32 col; };
    static const PassInfo kPasses[] = {
        { "D3D12 Core", 0.12f, IM_COL32(139,139,150,255) },
        { "Env/IBL", 0.34f, IM_COL32( 98,176,201,255) },
        { "Skinning", 0.61f, IM_COL32(111,206,154,255) },
        { "G-Buffer", 2.18f, IM_COL32(232,146, 74,255) },
        { "Deferred", 3.42f, IM_COL32(232, 96,110,255) },
        { "Forward", 1.27f, IM_COL32(217,162, 62,255) },
        { "Debug", 0.21f, IM_COL32(155,123,208,255) },
        { "RenderTex", 0.44f, IM_COL32( 90,166,232,255) },
        { "ImGui", 0.38f, IM_COL32(199,199,207,255) },
    };
    static constexpr int kN = (int)(sizeof(kPasses) / sizeof(kPasses[0]));

    float refTotal = 0.f;
    for (auto& p : kPasses) refTotal += p.refMs;

    const bool gpuReady = m_editor->isGpuTimerReady();
    const float gpuMs = gpuReady ? (float)m_editor->getGpuFrameTimeMs() : refTotal;
    const float cpuMs = app->getAvgElapsedMs();
    const float fps = app->getFPS();
    const float scale = gpuMs / refTotal;
    const float budgetPc = std::min(100.f, gpuMs / 16.67f * 100.f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float winW = ImGui::GetContentRegionAvail().x;

    {
        ImGui::PushFont(g_fontMono);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::TextUnformatted("GPU FRAME");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 8);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::msColor(gpuMs));
        ImGui::SetWindowFontScale(1.6f);
        ImGui::Text("%.2f", gpuMs);
        ImGui::SetWindowFontScale(1.f);
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 2);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::TextUnformatted("ms");
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 20);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::TextUnformatted("CPU");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::msColor(cpuMs));
        ImGui::Text("%.2f ms", cpuMs);
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 20);
        const ImVec4 bCol = budgetPc < 70.f ? EditorColors::Ok
                          : budgetPc < 90.f ? EditorColors::Warn : EditorColors::Crit;
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::TextUnformatted("BUDGET");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);
        ImGui::PushStyleColor(ImGuiCol_Text, bCol);
        ImGui::Text("%.0f%%", budgetPc);
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 20);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Ok);
        ImGui::Text("%.0f fps", fps);
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 20);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::Text("Draw Calls: %d", m_editor->getFrameDrawCalls());
        ImGui::PopStyleColor();

        ImGui::PopFont();
    }

    ImGui::Spacing();

    const float barH = 24.f;
    const float barY = ImGui::GetCursorScreenPos().y;
    const float barX = ImGui::GetCursorScreenPos().x;
    const float barW = winW;

    dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
        ImGui::ColorConvertFloat4ToU32(EditorColors::Bg2), 4.f);

    float xOff = 0.f;
    for (int i = 0; i < kN; ++i){
        float w = (kPasses[i].refMs / refTotal) * barW;
        float passMs = kPasses[i].refMs * scale;
        dl->AddRectFilled(ImVec2(barX + xOff, barY),
                          ImVec2(barX + xOff + w, barY + barH),
                          kPasses[i].col, (i == 0 ? 4.f : 0.f));
        if (w > 32.f){
            char lbl[16]; snprintf(lbl, sizeof(lbl), "%.2f", passMs);
            ImVec2 ts = ImGui::CalcTextSize(lbl);
            dl->AddText(ImVec2(barX + xOff + (w - ts.x) * 0.5f, barY + (barH - ts.y) * 0.5f),
                IM_COL32(0, 0, 0, 190), lbl);
        }
        if (i < kN - 1)
            dl->AddLine(ImVec2(barX + xOff + w, barY),
                        ImVec2(barX + xOff + w, barY + barH), IM_COL32(0,0,0,80));
        xOff += w;
    }

    if (gpuMs > 0.f){
        float bx = barX + std::min(1.f, 16.67f / gpuMs) * barW;
        dl->AddLine(ImVec2(bx, barY - 3.f), ImVec2(bx, barY + barH + 3.f),
            ImGui::ColorConvertFloat4ToU32(EditorColors::Crit), 2.f);
    }

    ImGui::Dummy(ImVec2(barW, barH + 4.f));

    ImGui::PushFont(g_fontMono);
    float lx = barX;
    for (int i = 0; i < kN; ++i){
        float segW = (kPasses[i].refMs / refTotal) * barW;
        float passMs = kPasses[i].refMs * scale;
        char lbl[64]; snprintf(lbl, sizeof(lbl), "%s %.2f", kPasses[i].name, passMs);
        ImVec2 ts = ImGui::CalcTextSize(lbl);

        ImVec2 cp = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(ImVec2(lx, cp.y + 3.f), ImVec2(lx + 7.f, cp.y + 9.f), kPasses[i].col, 1.f);
        ImGui::SetCursorScreenPos(ImVec2(lx + 9.f, cp.y));
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::TextUnformatted(lbl);
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 14);

        lx += std::max(segW, ts.x + 14.f);
        if (lx > barX + barW) break;
    }
    ImGui::NewLine();
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::TextUnformatted("FPS History");
    ImGui::PopStyleColor();
    float ordered[kHistory];
    float maxFPS = 60.0f;
    for (int i = 0; i < kHistory; ++i){
        ordered[i] = m_fpsHistory[(m_fpsIdx + i) % kHistory];
        if (ordered[i] > maxFPS) maxFPS = ordered[i];
    }
    ImGui::PushStyleColor(ImGuiCol_PlotLines, EditorColors::Ok);
    ImGui::PlotLines("##fps", ordered, kHistory, 0, nullptr, 0.f, maxFPS * 1.1f, ImVec2(-1, 60));
    ImGui::PopStyleColor();
}
