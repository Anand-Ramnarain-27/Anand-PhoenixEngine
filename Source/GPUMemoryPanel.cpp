#include "Globals.h"
#include "GPUMemoryPanel.h"
#include "EditorColors.h"
#include "ImGuiPass.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleStaticBuffer.h"
#include "ModuleRingBuffer.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleRTDescriptors.h"
#include "ModuleDSDescriptors.h"
#include <algorithm>

void GPUMemoryPanel::drawBar(const char* , float used, float total, ImU32 color){
    const float barH = 14.f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float fill = (total > 0.f) ? std::min(1.f, used / total) * w : 0.f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + barH), ImGui::ColorConvertFloat4ToU32(EditorColors::Bg0), 3.f);
    ImU32 c0 = (color & 0x00FFFFFF) | ((((color >> 24) * 204u) / 255u) << 24);
    dl->AddRectFilled(p, ImVec2(p.x + fill, p.y + barH), c0, 3.f);
    dl->AddRectFilled(ImVec2(p.x + fill * 0.5f, p.y), ImVec2(p.x + fill, p.y + barH), color, 0.f);
    for (int i = 1; i < 12; ++i){
        float gx = p.x + w * i / 11.f;
        dl->AddLine(ImVec2(gx, p.y), ImVec2(gx, p.y + barH), IM_COL32(0, 0, 0, 40));
    }
    dl->AddRect(p, ImVec2(p.x + w, p.y + barH), ImGui::ColorConvertFloat4ToU32(EditorColors::Line), 3.f);
    ImGui::Dummy(ImVec2(w, barH));
}

void GPUMemoryPanel::drawContent(){
    const float labelW = 118.f;
    const float valW = 96.f;

    ModuleStaticBuffer* sb = app->getStaticBuffer();
    ModuleD3D12* d3d12 = app->getD3D12();

    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::TextUnformatted("DESCRIPTOR HEAPS");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        float vramUsed = 0.f, vramTotal = 0.f;
        if (d3d12){
            DXGI_QUERY_VIDEO_MEMORY_INFO vi = d3d12->getLocalVideoMemoryInfo();
            vramUsed = vi.CurrentUsage / (1024.f*1024.f*1024.f);
            vramTotal = vi.Budget / (1024.f*1024.f*1024.f);
        }
        char vramBuf[64];
        snprintf(vramBuf, sizeof(vramBuf), "resident %.2f / %.1f GB VRAM", vramUsed, vramTotal);
        float tw = ImGui::CalcTextSize(vramBuf).x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - tw - 10.f);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::PushFont(g_fontMono);
        ImGui::TextUnformatted(vramBuf);
        ImGui::PopFont();
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();

    ModuleShaderDescriptors* shaderDesc = app->getShaderDescriptors();
    ModuleSamplerHeap* samplerHeap = app->getSamplerHeap();
    ModuleRTDescriptors* rtDesc = app->getRTDescriptors();
    ModuleDSDescriptors* dsDesc = app->getDSDescriptors();

    struct HeapRow { const char* name; const char* sub; float used; float total; const char* unit; ImU32 color; };
    HeapRow heaps[] = {
        { "CBV / SRV / UAV", "shader-visible",
          shaderDesc ? float(shaderDesc->getUsedTables()) : 0.f,
          shaderDesc ? float(shaderDesc->getTotalTables()) : 0.f, "tbl", ImGui::ColorConvertFloat4ToU32(EditorColors::Inf) },
        { "Sampler", "shader-visible",
          samplerHeap ? float(samplerHeap->getUsedSamplers()) : 0.f,
          samplerHeap ? float(samplerHeap->getTotalSamplers()) : 0.f, "desc", ImGui::ColorConvertFloat4ToU32(EditorColors::Ok) },
        { "RTV", "cpu",
          rtDesc ? float(rtDesc->getUsedDescriptors()) : 0.f,
          rtDesc ? float(rtDesc->getTotalDescriptors()) : 0.f, "desc", ImGui::ColorConvertFloat4ToU32(EditorColors::Hot) },
        { "DSV", "cpu",
          dsDesc ? float(dsDesc->getUsedDescriptors()) : 0.f,
          dsDesc ? float(dsDesc->getTotalDescriptors()) : 0.f, "desc", ImGui::ColorConvertFloat4ToU32(EditorColors::Acc) },
    };

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 4.f, 4.f });
    if (ImGui::BeginTable("##heaps", 3, ImGuiTableFlags_SizingFixedFit)){
        ImGui::TableSetupColumn("##n", ImGuiTableColumnFlags_WidthFixed, labelW);
        ImGui::TableSetupColumn("##b", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthFixed, valW);
        for (auto& h : heaps){
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
            ImGui::Text("%s", h.name);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            drawBar(h.name, h.used, h.total, h.color);
            ImGui::TableSetColumnIndex(2);
            ImGui::PushFont(g_fontMono);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
            ImGui::Text("%.0f/%.0f", h.used, h.total);
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::TextUnformatted("UPLOAD RING BUFFER");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    {
        ModuleRingBuffer* ring = app->getRingBuffer();
        const size_t ringUsed = ring ? ring->getUsedBytes() : 0;
        const size_t ringCap = ring ? ring->getCapacity() : 0;
        const float radius = 40.f;
        const float thick = 9.f;
        const float rpc = ringCap ? std::min(1.f, float(ringUsed) / float(ringCap)) : 0.f;
        ImVec2 center = { ImGui::GetCursorScreenPos().x + radius + 8.f,
                          ImGui::GetCursorScreenPos().y + radius + 4.f };
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddCircle(center, radius, ImGui::ColorConvertFloat4ToU32(EditorColors::Bg3), 64, thick);
        const int kSegs = 64;
        float sweepEnd = -IM_PI * 0.5f + IM_PI * 2.f * rpc;
        dl->PathArcTo(center, radius, -IM_PI * 0.5f, sweepEnd, kSegs);
        dl->PathStroke(ImGui::ColorConvertFloat4ToU32(EditorColors::Inf), 0, thick);
        ImGui::PushFont(g_fontMono);
        char pct[8]; snprintf(pct, sizeof(pct), "%.0f%%", rpc * 100.f);
        ImVec2 ts = ImGui::CalcTextSize(pct);
        dl->AddText({ center.x - ts.x * 0.5f, center.y - ts.y * 0.5f - 4.f },
            ImGui::ColorConvertFloat4ToU32(EditorColors::Tx0), pct);
        char sub[24];
        snprintf(sub, sizeof(sub), "%.1f/%.0fMB",
            ringUsed / (1024.f * 1024.f), ringCap / (1024.f * 1024.f));
        ImVec2 ss = ImGui::CalcTextSize(sub);
        dl->AddText({ center.x - ss.x * 0.5f, center.y + ts.y * 0.5f - 2.f },
            ImGui::ColorConvertFloat4ToU32(EditorColors::Tx2), sub);
        ImGui::PopFont();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + radius * 2.f + 20.f);
        ImGui::BeginGroup();

        const float fBW = 40.f, fBH = 18.f;
        ImVec2 fStart = ImGui::GetCursorScreenPos();
        const uint32_t frameCount = ModuleRingBuffer::getFrameCount();
        const uint32_t curFrame = ring ? ring->getCurrentFrame() : 0;
        // Scale fill against an even per-frame share of the ring so bars stay comparable.
        const float perFrameBudget = (ring && frameCount) ? float(ringCap) / float(frameCount) : 1.f;
        const ImU32 frameCols[3] = {
            IM_COL32( 58,106,140,255), IM_COL32( 74,168,232,255), IM_COL32(121,192,255,255) };
        float fx = fStart.x;
        for (uint32_t i = 0; i < frameCount; ++i){
            bool isCur = (i == curFrame);
            float frameBytes = ring ? float(ring->getFrameBytes(i)) : 0.f;
            char fname[8]; snprintf(fname, sizeof(fname), "F%u", i);
            dl->AddRectFilled({ fx, fStart.y }, { fx + fBW, fStart.y + fBH },
                ImGui::ColorConvertFloat4ToU32(EditorColors::Bg0), 3.f);
            float frac = perFrameBudget > 0.f ? std::min(1.f, frameBytes / perFrameBudget) : 0.f;
            float fillH = fBH * frac;
            dl->AddRectFilled({ fx, fStart.y + fBH - fillH }, { fx + fBW, fStart.y + fBH },
                frameCols[i % 3], 0.f);
            ImU32 border = isCur ? ImGui::ColorConvertFloat4ToU32(EditorColors::Acc) :
                                   ImGui::ColorConvertFloat4ToU32(EditorColors::Line2);
            dl->AddRect({ fx, fStart.y }, { fx + fBW, fStart.y + fBH }, border, 3.f);
            ImVec2 lt = ImGui::CalcTextSize(fname);
            dl->AddText({ fx + (fBW - lt.x) * 0.5f, fStart.y + 3.f },
                ImGui::ColorConvertFloat4ToU32(EditorColors::Tx2), fname);
            fx += fBW + 4.f;
        }
        ImGui::Dummy(ImVec2(fx - fStart.x, fBH + 4.f));

        ImGui::PushFont(g_fontMono);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        char headBuf[24]; snprintf(headBuf, sizeof(headBuf), "0x%llX",
            ring ? (unsigned long long)ring->getHead() : 0ull);
        char tailBuf[24]; snprintf(tailBuf, sizeof(tailBuf), "0x%llX",
            ring ? (unsigned long long)ring->getTail() : 0ull);
        char budgetBuf[24]; snprintf(budgetBuf, sizeof(budgetBuf), "%.2f MB",
            perFrameBudget / (1024.f * 1024.f));
        ImGui::Text("head offset"); ImGui::SameLine(80.f);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
        ImGui::TextUnformatted(headBuf);
        ImGui::PopStyleColor();
        ImGui::Text("tail offset"); ImGui::SameLine(80.f);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
        ImGui::TextUnformatted(tailBuf);
        ImGui::PopStyleColor();
        ImGui::Text("per-frame budget"); ImGui::SameLine(80.f);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
        ImGui::TextUnformatted(budgetBuf);
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::EndGroup();
    }
    float ringGroupH = ImGui::GetItemRectSize().y;
    if (ringGroupH < 88.f) ImGui::Dummy(ImVec2(0.f, 88.f - ringGroupH));

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::TextUnformatted("STATIC BUFFER POOLS");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    struct PoolRow { const char* name; float used; float total; const char* unit; ImU32 col; };
    float sbUsedMB = sb ? sb->getUsedBytes() / (1024.f * 1024.f) : 0.f;
    float sbTotalMB = sb ? sb->getTotalBytes() / (1024.f * 1024.f) : 0.f;
    ModuleRingBuffer* ringPool = app->getRingBuffer();
    float ringUsedMB = ringPool ? ringPool->getUsedBytes() / (1024.f * 1024.f) : 0.f;
    float ringTotalMB = ringPool ? ringPool->getCapacity() / (1024.f * 1024.f) : 0.f;
    PoolRow pools[] = {
        { "Static Vertex/Index", sbUsedMB, sbTotalMB, "MB", ImGui::ColorConvertFloat4ToU32(EditorColors::Ok) },
        { "Upload Ring Buffer", ringUsedMB, ringTotalMB, "MB", ImGui::ColorConvertFloat4ToU32(EditorColors::Hot) },
    };

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 4.f, 4.f });
    if (ImGui::BeginTable("##pools", 3, ImGuiTableFlags_SizingFixedFit)){
        ImGui::TableSetupColumn("##pn", ImGuiTableColumnFlags_WidthFixed, labelW);
        ImGui::TableSetupColumn("##pb", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##pv", ImGuiTableColumnFlags_WidthFixed, valW);
        for (auto& p : pools){
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
            ImGui::Text("%s", p.name);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            drawBar(p.name, p.used, p.total, p.col);
            ImGui::TableSetColumnIndex(2);
            ImGui::PushFont(g_fontMono);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
            ImGui::Text("%.0f/%.0f", p.used, p.total);
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}
