#include "Globals.h"
#include "GPUMemoryPanel.h"
#include "EditorColors.h"
#include "ImGuiPass.h"
#include "Application.h"
#include "ModuleStaticBuffer.h"
#include "ModuleRingBuffer.h"

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

    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::TextUnformatted("DESCRIPTOR HEAPS");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        float vramUsed = 0.f, vramTotal = 0.f;
        if (sb){ vramUsed = sb->getUsedBytes() / (1024.f*1024.f*1024.f);
                  vramTotal = sb->getTotalBytes() / (1024.f*1024.f*1024.f); }
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

    struct HeapRow { const char* name; const char* sub; float used; float total; const char* unit; ImU32 color; };
    HeapRow heaps[] = {
        { "CBV / SRV / UAV", "shader-visible", 2310.f, 4096.f, "desc", ImGui::ColorConvertFloat4ToU32(EditorColors::Inf) },
        { "Sampler", "shader-visible", 48.f, 256.f, "desc", ImGui::ColorConvertFloat4ToU32(EditorColors::Ok) },
        { "RTV", "cpu", 36.f, 128.f, "desc", ImGui::ColorConvertFloat4ToU32(EditorColors::Hot) },
        { "DSV", "cpu", 12.f, 64.f, "desc", ImGui::ColorConvertFloat4ToU32(EditorColors::Acc) },
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
        const float radius = 40.f;
        const float thick = 9.f;
        const float rpc = 0.575f;
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
        char sub[] = "18.4/32MB";
        ImVec2 ss = ImGui::CalcTextSize(sub);
        dl->AddText({ center.x - ss.x * 0.5f, center.y + ts.y * 0.5f - 2.f },
            ImGui::ColorConvertFloat4ToU32(EditorColors::Tx2), sub);
        ImGui::PopFont();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + radius * 2.f + 20.f);
        ImGui::BeginGroup();

        const float fBW = 40.f, fBH = 18.f;
        ImVec2 fStart = ImGui::GetCursorScreenPos();
        struct Frame { const char* n; float mb; ImU32 col; };
        Frame frames[] = {
            { "N-2", 6.1f, IM_COL32( 58,106,140,255) },
            { "N-1", 7.0f, IM_COL32( 74,168,232,255) },
            { "N", 5.3f, IM_COL32(121,192,255,255) },
        };
        float fx = fStart.x;
        for (int i = 0; i < 3; ++i){
            bool isCur = (i == 1);
            dl->AddRectFilled({ fx, fStart.y }, { fx + fBW, fStart.y + fBH },
                ImGui::ColorConvertFloat4ToU32(EditorColors::Bg0), 3.f);
            float fillH = fBH * frames[i].mb / 10.f;
            dl->AddRectFilled({ fx, fStart.y + fBH - fillH }, { fx + fBW, fStart.y + fBH },
                frames[i].col, 0.f);
            ImU32 border = isCur ? ImGui::ColorConvertFloat4ToU32(EditorColors::Acc) :
                                   ImGui::ColorConvertFloat4ToU32(EditorColors::Line2);
            dl->AddRect({ fx, fStart.y }, { fx + fBW, fStart.y + fBH }, border, 3.f);
            ImVec2 lt = ImGui::CalcTextSize(frames[i].n);
            dl->AddText({ fx + (fBW - lt.x) * 0.5f, fStart.y + 3.f },
                ImGui::ColorConvertFloat4ToU32(EditorColors::Tx2), frames[i].n);
            fx += fBW + 4.f;
        }
        ImGui::Dummy(ImVec2(fx - fStart.x, fBH + 4.f));

        ImGui::PushFont(g_fontMono);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::Text("head offset"); ImGui::SameLine(80.f);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
        ImGui::TextUnformatted("0x1266666");
        ImGui::PopStyleColor();
        ImGui::Text("per-frame budget"); ImGui::SameLine(80.f);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
        ImGui::TextUnformatted("10.67 MB");
        ImGui::PopStyleColor();
        ImGui::Text("wraps / sec"); ImGui::SameLine(80.f);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
        ImGui::TextUnformatted("2.1");
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
    float sbUsedMB = sb ? sb->getUsedBytes() / (1024.f * 1024.f) : 412.f;
    float sbTotalMB = sb ? sb->getTotalBytes() / (1024.f * 1024.f) : 512.f;
    PoolRow pools[] = {
        { "Static Vertex/Index", sbUsedMB, sbTotalMB, "MB", ImGui::ColorConvertFloat4ToU32(EditorColors::Ok) },
        { "Texture Streaming", 2840.f, 4096.f, "MB", ImGui::ColorConvertFloat4ToU32(EditorColors::Hot) },
        { "Readback", 6.f, 16.f, "MB", ImGui::ColorConvertFloat4ToU32(EditorColors::Acc) },
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
