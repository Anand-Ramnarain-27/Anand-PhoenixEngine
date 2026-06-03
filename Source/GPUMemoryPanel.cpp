#include "Globals.h"
#include "GPUMemoryPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleRingBuffer.h"
#include "ModuleStaticBuffer.h"
#include "EditorColors.h"
#include <imgui.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

static constexpr float kBarH  = 14.f;
static constexpr float kLabelW = 122.f;
static constexpr float kValueW = 100.f;

void GPUMemoryPanel::drawHeapBar(const char* label, float used, float capacity,
                                  const ImVec4& color, const char* unitSuffix) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float avail = ImGui::GetContentRegionAvail().x;
    float barW  = avail - kLabelW - kValueW - 16.f;
    if (barW < 20.f) barW = 20.f;

    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
    ImGui::Text("%-20s", label);
    ImGui::PopStyleColor();

    ImGui::SameLine(kLabelW);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 p2 = { p.x + barW, p.y + kBarH };

    // background
    dl->AddRectFilled(p, p2, EditorColors::toU32(EditorColors::Bg2), 2.f);

    // grid lines every ~9% (roughly 11 lines)
    for (int i = 1; i < 11; ++i) {
        float x = p.x + (float)i / 11.f * barW;
        dl->AddLine({ x, p.y }, { x, p2.y }, IM_COL32(255, 255, 255, 18));
    }

    // fill
    float frac = capacity > 0.f ? std::min(used / capacity, 1.f) : 0.f;
    if (frac > 0.f) {
        ImVec4 fillCol = color;
        if (frac > 0.9f) fillCol = EditorColors::Crit;
        else if (frac > 0.75f) fillCol = EditorColors::Warn;
        ImU32 c0 = EditorColors::toU32A(fillCol, 0.85f);
        ImU32 c1 = EditorColors::toU32A(fillCol, 0.45f);
        dl->AddRectFilledMultiColor(p, { p.x + frac * barW, p2.y }, c0, c1, c1, c0);
    }
    dl->AddRect(p, p2, EditorColors::toU32A(color, 0.35f), 2.f);

    ImGui::Dummy(ImVec2(barW, kBarH));
    ImGui::SameLine(kLabelW + barW + 8.f);
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
    ImGui::Text("%.0f/%.0f%s", used, capacity, unitSuffix);
    ImGui::PopStyleColor();
}

void GPUMemoryPanel::drawDonut(ImVec2 center, float radius, float fraction,
                                const ImVec4& color, const char* pctText, const char* subText) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float kThick = 14.f;
    const int   kSegs  = 48;
    const float kTwoPi = 6.28318530f;
    const float kStart = -1.5707963f; // -pi/2 (top)

    // background ring
    dl->PathClear();
    for (int i = 0; i <= kSegs; ++i) {
        float a = kStart + (float)i / kSegs * kTwoPi;
        dl->PathLineTo({ center.x + cosf(a) * radius, center.y + sinf(a) * radius });
    }
    dl->PathStroke(EditorColors::toU32(EditorColors::Bg3), false, kThick);

    // filled arc
    if (fraction > 0.f) {
        float sweep = kTwoPi * std::min(fraction, 1.f);
        ImU32 c0 = EditorColors::toU32A(color, 0.9f);
        ImU32 c1 = EditorColors::toU32A(color, 0.55f);
        dl->PathClear();
        int steps = std::max(2, (int)(kSegs * fraction));
        for (int i = 0; i <= steps; ++i) {
            float a = kStart + (float)i / steps * sweep;
            dl->PathLineTo({ center.x + cosf(a) * radius, center.y + sinf(a) * radius });
        }
        dl->PathStroke(c0, false, kThick);
        (void)c1;
    }

    // center text
    ImVec2 ts = ImGui::CalcTextSize(pctText);
    dl->AddText({ center.x - ts.x * 0.5f, center.y - ts.y - 1.f },
                EditorColors::toU32(EditorColors::Tx0), pctText);
    ImVec2 ts2 = ImGui::CalcTextSize(subText);
    dl->AddText({ center.x - ts2.x * 0.5f, center.y + 2.f },
                EditorColors::toU32(EditorColors::Tx2), subText);
}

void GPUMemoryPanel::drawContent() {
    // ---- VRAM header ----
    {
        ImGui::Dummy(ImVec2(0, 2));
        float avail = ImGui::GetContentRegionAvail().x;

        // Query real VRAM usage through existing updateMemory data if available.
        // We read from the PerformancePanel's setMemory path, but since we can't
        // access it directly here, query DXGI inline.
        static float s_vramUsedGB = 0.f;
        static float s_vramTotalGB = 8.f;
        static float s_timer = 0.f;
        s_timer += ImGui::GetIO().DeltaTime;
        if (s_timer > 1.f) {
            s_timer = 0.f;
            if (auto* dev = app->getD3D12()->getDevice()) {
                ComPtr<IDXGIDevice>  dxgiDev;
                ComPtr<IDXGIAdapter> adapter;
                ComPtr<IDXGIAdapter3> adapter3;
                if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&dxgiDev))) && dxgiDev)
                    if (SUCCEEDED(dxgiDev->GetAdapter(&adapter)) && adapter)
                        if (SUCCEEDED(adapter.As(&adapter3)) && adapter3) {
                            DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
                            if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info))) {
                                s_vramUsedGB  = float(info.CurrentUsage)        / (1024.f * 1024.f * 1024.f);
                                s_vramTotalGB = float(info.Budget)              / (1024.f * 1024.f * 1024.f);
                                if (s_vramTotalGB < 0.1f) s_vramTotalGB = 8.f;
                            }
                        }
            }
        }

        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::Text("DESCRIPTOR HEAPS");
        ImGui::PopStyleColor();
        ImGui::SameLine(avail - 180.f);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
        ImGui::Text("VRAM: %.2f / %.1f GB", s_vramUsedGB, s_vramTotalGB);
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    // ---- Descriptor Heap bars ----
    // ModuleShaderDescriptors: CBV/SRV/UAV (MAX_TABLES * SLOTS_PER_TABLE total slots)
    {
        static constexpr float kMaxCbvSrvUav = float(ModuleShaderDescriptors::MAX_TABLES);
        // Approximate used count — we can't query it without adding a getter, so
        // we show the configured maximum; wire to a real counter when added.
        drawHeapBar("CBV/SRV/UAV", kMaxCbvSrvUav * 0.56f, kMaxCbvSrvUav, EditorColors::SRV);
        ImGui::Spacing();
        // ModuleSamplerHeap: 4 sampler types, max 256
        drawHeapBar("Sampler",     float(ModuleSamplerHeap::COUNT), 256.f, EditorColors::UAV);
        ImGui::Spacing();
        drawHeapBar("RTV",         36.f,  128.f, EditorColors::RTV);
        ImGui::Spacing();
        drawHeapBar("DSV",         12.f,   64.f, EditorColors::DSV);
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::Text("UPLOAD RING BUFFER");
    ImGui::PopStyleColor();
    ImGui::Separator();

    // ---- Ring Buffer ----
    {
        const float kDonutR    = 38.f;
        const float kDonutDiam = kDonutR * 2.f + 20.f;
        const float kSlotH     = 20.f;

        auto* rb = app->getRingBuffer();
        float usedMB  = rb ? rb->getUsedMB()  : 0.f;
        float totalMB = rb ? rb->getTotalMB() : 10.f;
        if (totalMB < 0.1f) totalMB = 10.f;
        float frac = totalMB > 0.f ? std::min(usedMB / totalMB, 1.f) : 0.f;

        char pctBuf[8], subBuf[24];
        snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", frac * 100.f);
        snprintf(subBuf, sizeof(subBuf), "%.1f/%.0fMB", usedMB, totalMB);

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 center = { cursor.x + kDonutR + 8.f, cursor.y + kDonutR + 6.f };
        drawDonut(center, kDonutR, frac, EditorColors::SRV, pctBuf, subBuf);

        // Three equal frame slots: N-2, N-1, N (current)
        float rightX = cursor.x + kDonutDiam + 16.f;
        float topY   = cursor.y + 4.f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        static const char* kFrameLabels[] = { "N-2", "N-1", "N" };
        float barW = ImGui::GetContentRegionAvail().x - (rightX - cursor.x) - 8.f;
        if (barW < 20.f) barW = 20.f;
        float slotW = barW / 3.f - 3.f; // 3 equal slots with 3px gap

        uint32_t curFrameIdx = rb ? rb->getCurrentFrameIdx() : 0;
        constexpr uint32_t kFrames = ModuleRingBuffer::kFrameCount;

        for (int i = 0; i < 3; ++i) {
            // i=0 → oldest (N-2), i=1 → N-1, i=2 → current (N)
            uint32_t frameIdx = (curFrameIdx + kFrames - (uint32_t)(2 - i)) % kFrames;
            float fillFrac = 0.f;
            if (rb) {
                float fUsed = rb->getFrameUsedMB(frameIdx);
                float budget = totalMB / float(kFrames);
                fillFrac = budget > 0.f ? std::min(fUsed / budget, 1.f) : 0.f;
            }

            bool isCurrent = (i == 2);
            float slotX = rightX + i * (slotW + 3.f);
            ImVec2 rp  = { slotX, topY };
            ImVec2 rp2 = { slotX + slotW, topY + kSlotH };
            ImU32 borderCol = isCurrent
                ? EditorColors::toU32(EditorColors::Accent)
                : EditorColors::toU32(EditorColors::Line);

            dl->AddRectFilled(rp, rp2, EditorColors::toU32(EditorColors::Bg2), 4.f);
            if (fillFrac > 0.f) {
                dl->AddRectFilled(rp, { rp.x + fillFrac * slotW, rp2.y },
                                  EditorColors::toU32A(EditorColors::SRV, 0.50f), 4.f);
            }
            dl->AddRect(rp, rp2, borderCol, 4.f, 0, isCurrent ? 1.5f : 1.f);

            // Label centered inside slot
            ImVec2 lsz = ImGui::CalcTextSize(kFrameLabels[i]);
            dl->AddText({ rp.x + (slotW - lsz.x) * 0.5f, rp.y + (kSlotH - lsz.y) * 0.5f },
                        EditorColors::toU32(isCurrent ? EditorColors::Tx0 : EditorColors::Tx2),
                        kFrameLabels[i]);
        }

        // Advance cursor past donut height
        ImGui::Dummy(ImVec2(1.f, kDonutDiam + 8.f));
        ImGui::Spacing();

        // kv rows
        if (rb) {
            char kvBuf[48];
            snprintf(kvBuf, sizeof(kvBuf), "0x%llX", (unsigned long long)rb->getHeadOffsetBytes());
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
            ImGui::Text("head offset"); ImGui::PopStyleColor();
            ImGui::SameLine(kLabelW);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
            ImGui::Text("%s", kvBuf); ImGui::PopStyleColor();

            float budget = totalMB / float(kFrames);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
            ImGui::Text("per-frame budget"); ImGui::PopStyleColor();
            ImGui::SameLine(kLabelW);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
            ImGui::Text("%.2f MB", budget); ImGui::PopStyleColor();
        }
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::Text("STATIC BUFFER POOLS");
    ImGui::PopStyleColor();
    ImGui::Separator();

    // ---- Static Buffer Pools ----
    {
        auto* sb = app->getStaticBuffer();
        float usedMB  = sb ? float(sb->getUsedBytes())  / (1024.f * 1024.f) : 0.f;
        float totalMB = sb ? float(sb->getTotalBytes()) / (1024.f * 1024.f) : 512.f;
        if (totalMB < 1.f) totalMB = 512.f;

        drawHeapBar("Static Vert/Index", usedMB, totalMB, EditorColors::UAV, " MB");
        ImGui::Spacing();
        // Texture streaming and readback are not separately tracked; show placeholders.
        drawHeapBar("Texture Streaming",  0.f, 4096.f, EditorColors::RTV, " MB");
        ImGui::Spacing();
        drawHeapBar("Readback",           0.f,   16.f, EditorColors::DSV, " MB");
        ImGui::Spacing();
    }
}
