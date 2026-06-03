#include "Globals.h"
#include "RenderGraphPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "EditorColors.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <algorithm>

RenderGraphPanel::RenderGraphPanel(ModuleEditor* editor) : EditorPanel(editor) {
    buildPassData();
}

// Map resource name to its type color (RTV/SRV/UAV/DSV/CBV).
static ImVec4 resourceColor(const char* name) {
    // CBV (constant buffers)
    if (strstr(name, "CB") || strstr(name, "Const") || strstr(name, "Frame")) return EditorColors::CBV;
    // DSV (depth surfaces)
    if (strstr(name, "Depth") || strstr(name, "DSV"))  return EditorColors::DSV;
    // UAV (unordered access)
    if (strstr(name, "VB") || strstr(name, "UAV"))     return EditorColors::UAV;
    // SRV (shader resource views)
    if (strstr(name, "IBL") || strstr(name, "Env") || strstr(name, "LUT") || strstr(name, "SRV"))
        return EditorColors::SRV;
    // RTV (render targets) — GBuffer textures, HDR color, Backbuffer, SceneRT
    return EditorColors::RTV;
}

void RenderGraphPanel::buildPassData() {
    // port: { name, isOutput }
    struct RawPass {
        int id; const char* name; const char* badge;
        float r, g, b; float ms;
        std::vector<std::pair<const char*, bool>> ports;
    };

    static const RawPass kPasses[] = {
        { 0, "D3D12 Core",       "COMP", 0.545f,0.545f,0.588f, 0.12f,
          {{"FrameCB",true},{"Backbuffer",true}} },
        { 1, "Environment/IBL",  "COMP", 0.384f,0.690f,0.788f, 0.34f,
          {{"FrameCB",false},{"IBLCube",true},{"IBL-LUT",true}} },
        { 2, "Skinning",         "COMP", 0.435f,0.808f,0.604f, 0.61f,
          {{"FrameCB",false},{"SkinnedVB",true}} },
        { 3, "G-Buffer",         "GFX",  0.910f,0.573f,0.290f, 2.18f,
          {{"SkinnedVB",false},{"GBuffer.A",true},{"GBuffer.B",true},{"GBuffer.C",true},{"SceneDepth",true}} },
        { 4, "Deferred Light",   "GFX",  0.910f,0.376f,0.431f, 3.42f,
          {{"FrameCB",false},{"GBuffer.A",false},{"GBuffer.B",false},{"GBuffer.C",false},
           {"SceneDepth",false},{"IBLCube",false},{"HDRColor",true}} },
        { 5, "Forward Mesh",     "GFX",  0.851f,0.635f,0.243f, 1.27f,
          {{"FrameCB",false},{"HDRColor",false},{"SceneDepth",false},{"IBLCube",false},{"HDRColor",true}} },
        { 6, "Debug Draw",       "GFX",  0.608f,0.482f,0.816f, 0.21f,
          {{"HDRColor",false},{"SceneDepth",false},{"HDRColor",true}} },
        { 7, "Render Texture",   "GFX",  0.353f,0.651f,0.910f, 0.44f,
          {{"HDRColor",false},{"Backbuffer",true}} },
        { 8, "ImGui",            "GFX",  0.780f,0.780f,0.812f, 0.38f,
          {{"Backbuffer",false},{"Backbuffer",true}} },
    };

    m_passes.clear();
    for (const auto& r : kPasses) {
        PassNode n;
        n.id       = r.id;
        n.name     = r.name;
        n.badge    = r.badge;
        n.dotColor = ImVec4(r.r, r.g, r.b, 1.f);
        n.gpuMs    = r.ms;
        for (const auto& [pname, isOut] : r.ports)
            n.ports.push_back({ pname, isOut, resourceColor(pname) });
        m_passes.push_back(std::move(n));
    }

    // Build wires from the spec resource flow; use resource type color per wire.
    // Each entry: { fromPassId, outPortName, toPassId, inPortName }
    struct WireSpec { int from; const char* res; int to; };
    static const WireSpec kWireSpecs[] = {
        { 0, "FrameCB",    2 }, { 0, "FrameCB",    4 }, { 0, "FrameCB",    5 },
        { 0, "Backbuffer", 7 },
        { 1, "IBLCube",    4 }, { 1, "IBLCube",    5 },
        { 2, "SkinnedVB",  3 },
        { 3, "GBuffer.A",  4 }, { 3, "GBuffer.B",  4 }, { 3, "GBuffer.C",  4 },
        { 3, "SceneDepth", 4 }, { 3, "SceneDepth", 5 }, { 3, "SceneDepth", 6 },
        { 4, "HDRColor",   5 },
        { 5, "HDRColor",   6 },
        { 6, "HDRColor",   7 },
        { 7, "Backbuffer", 8 },
    };

    m_wires.clear();
    for (const auto& ws : kWireSpecs) {
        // Find output port index in from-pass
        int fromPortIdx = -1;
        for (int pi = 0; pi < (int)m_passes[ws.from].ports.size(); ++pi)
            if (m_passes[ws.from].ports[pi].isOutput &&
                m_passes[ws.from].ports[pi].name == ws.res)
            { fromPortIdx = pi; break; }

        // Find input port index in to-pass
        int toPortIdx = -1;
        for (int pi = 0; pi < (int)m_passes[ws.to].ports.size(); ++pi)
            if (!m_passes[ws.to].ports[pi].isOutput &&
                m_passes[ws.to].ports[pi].name == ws.res)
            { toPortIdx = pi; break; }

        if (fromPortIdx >= 0 && toPortIdx >= 0) {
            ImVec4 wireCol = resourceColor(ws.res);
            m_wires.push_back({ ws.from, fromPortIdx, ws.to, toPortIdx, wireCol });
        }
    }
}

ImVec4 RenderGraphPanel::msColor(float ms) const {
    if (ms >= 3.0f) return EditorColors::Crit;
    if (ms >= 1.5f) return EditorColors::Hot;
    if (ms >= 0.5f) return EditorColors::Warn;
    return EditorColors::Ok;
}

static ImVec2 portPos(const RenderGraphPanel* /*rg*/, ImVec2 nodeOrigin,
                       int portIdx, int portCount, bool isOutput,
                       float nodeW, float nodeH) {
    float y = nodeOrigin.y + 32.f + (float)portIdx * (nodeH - 36.f) / std::max(portCount, 1);
    float x = isOutput ? nodeOrigin.x + nodeW : nodeOrigin.x;
    return { x, y };
}

void RenderGraphPanel::drawNode(const PassNode& node, ImVec2 origin, ImDrawList* dl, bool selected) {
    const float rounding = 6.f;
    ImVec2 br = { origin.x + kNodeW, origin.y + kNodeH };

    // Background
    ImU32 bgColor = selected
        ? EditorColors::toU32A(node.dotColor, 0.18f)
        : IM_COL32(26, 26, 32, 245);
    dl->AddRectFilled(origin, br, bgColor, rounding);
    ImU32 borderColor = selected
        ? EditorColors::toU32(node.dotColor)
        : EditorColors::toU32(EditorColors::Line);
    dl->AddRect(origin, br, borderColor, rounding, 0, selected ? 2.f : 1.f);

    // Header: [index] [dot] [name]  [badge]
    float hdrH = 24.f;
    ImVec2 hdrBr = { br.x, origin.y + hdrH };
    dl->AddRectFilled(origin, hdrBr,
                      EditorColors::toU32A(node.dotColor, 0.15f), rounding,
                      ImDrawFlags_RoundCornersTop);

    // Index badge
    char idxBuf[4]; snprintf(idxBuf, sizeof(idxBuf), "%02d", node.id);
    ImVec2 idxPos = { origin.x + 6.f, origin.y + 4.f };
    dl->AddText(idxPos, EditorColors::toU32(EditorColors::Tx2), idxBuf);

    // Colored dot
    dl->AddCircleFilled({ origin.x + 28.f, origin.y + hdrH * 0.5f }, 5.f,
                         EditorColors::toU32(node.dotColor));

    // Pass name
    dl->AddText({ origin.x + 36.f, origin.y + 4.f },
                EditorColors::toU32(EditorColors::Tx0), node.name.c_str());

    // GFX/COMP badge
    ImVec4 badgeCol = (node.badge == "COMP") ? EditorColors::SRV : EditorColors::RTV;
    float bw = ImGui::CalcTextSize(node.badge.c_str()).x + 8.f;
    ImVec2 bp = { br.x - bw - 4.f, origin.y + 4.f };
    dl->AddRectFilled(bp, { bp.x + bw, bp.y + 16.f },
                      EditorColors::toU32A(badgeCol, 0.25f), 3.f);
    dl->AddText({ bp.x + 4.f, bp.y }, EditorColors::toU32(badgeCol), node.badge.c_str());

    // GPU ms
    float ms = node.gpuMs;
    char msBuf[16]; snprintf(msBuf, sizeof(msBuf), "%.2f ms", ms);
    dl->AddText({ origin.x + 8.f, origin.y + hdrH + 4.f },
                EditorColors::toU32(msColor(ms)), msBuf);

    // Mini bar chart: 3 simulated frames
    {
        float barAreaX = origin.x + 8.f;
        float barAreaY = origin.y + hdrH + 18.f;
        float barAreaW = kNodeW - 16.f;
        float barAreaH = kNodeH - hdrH - 24.f;
        float barW = barAreaW / 5.f;
        static const float kFrameMultipliers[] = { 0.85f, 1.00f, 0.91f };
        for (int i = 0; i < 3; ++i) {
            float bh = barAreaH * std::min(kFrameMultipliers[i], 1.f);
            ImVec2 bb = { barAreaX + i * barW * 1.5f, barAreaY + barAreaH - bh };
            ImVec2 bb2 = { bb.x + barW, barAreaY + barAreaH };
            dl->AddRectFilled(bb, bb2,
                              (i == 1) ? EditorColors::toU32A(node.dotColor, 0.8f)
                                       : EditorColors::toU32A(node.dotColor, 0.4f), 2.f);
        }
    }

    // Port dots
    auto drawPorts = [&](bool isOutput) {
        int cnt = 0;
        for (const auto& p : node.ports) if (p.isOutput == isOutput) ++cnt;
        int idx = 0;
        for (int pi = 0; pi < (int)node.ports.size(); ++pi) {
            const auto& p = node.ports[pi];
            if (p.isOutput != isOutput) continue;
            ImVec2 pp = portPos(this, origin, idx, cnt, isOutput, kNodeW, kNodeH);
            dl->AddCircleFilled(pp, 4.f, EditorColors::toU32(p.color));
            dl->AddCircle(pp, 4.f, EditorColors::toU32A(p.color, 0.6f));
            // Label
            ImVec2 lt = ImGui::CalcTextSize(p.name.c_str());
            float tx = isOutput ? pp.x - lt.x - 6.f : pp.x + 6.f;
            dl->AddText({ tx, pp.y - lt.y * 0.5f },
                        EditorColors::toU32(EditorColors::Tx2), p.name.c_str());
            ++idx;
        }
    };
    drawPorts(false);
    drawPorts(true);
}

void RenderGraphPanel::drawWires(ImDrawList* dl, ImVec2 graphOrigin) {
    for (const auto& w : m_wires) {
        const PassNode& from = m_passes[w.fromPass];
        const PassNode& to   = m_passes[w.toPass];

        int fromOutCnt = 0, fromOutIdx = 0;
        for (int pi = 0; pi < (int)from.ports.size(); ++pi) {
            if (from.ports[pi].isOutput) {
                if (pi == w.fromPort) fromOutIdx = fromOutCnt;
                ++fromOutCnt;
            }
        }
        int toInCnt = 0, toInIdx = 0;
        for (int pi = 0; pi < (int)to.ports.size(); ++pi) {
            if (!to.ports[pi].isOutput) {
                if (pi == w.toPort) toInIdx = toInCnt;
                ++toInCnt;
            }
        }

        ImVec2 fOrig = { graphOrigin.x + w.fromPass * (kNodeW + kNodeGap), graphOrigin.y };
        ImVec2 tOrig = { graphOrigin.x + w.toPass   * (kNodeW + kNodeGap), graphOrigin.y };

        ImVec2 p1 = portPos(this, fOrig, fromOutIdx, fromOutCnt, true,  kNodeW, kNodeH);
        ImVec2 p2 = portPos(this, tOrig, toInIdx,    toInCnt,    false, kNodeW, kNodeH);

        bool highlight = (m_selectedPass == w.fromPass || m_selectedPass == w.toPass);
        float alpha    = highlight ? 1.0f : 0.62f;
        ImU32 col      = EditorColors::toU32A(w.color, alpha);

        float cp = (p2.x - p1.x) * 0.45f;
        dl->AddBezierCubic(p1,
                           { p1.x + cp, p1.y },
                           { p2.x - cp, p2.y },
                           p2, col, 1.5f, 24);
    }
}

void RenderGraphPanel::drawDetailStrip(float stripY, float stripH) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp      = ImGui::GetWindowPos();
    float  W       = ImGui::GetWindowSize().x;

    ImVec2 p  = { wp.x, wp.y + stripY };
    ImVec2 p2 = { wp.x + W, wp.y + stripY + stripH };
    dl->AddRectFilled(p, p2, IM_COL32(18, 18, 24, 240));
    dl->AddLine(p, { p2.x, p.y }, EditorColors::toU32(EditorColors::Line));

    float cy = p.y + (stripH - ImGui::GetTextLineHeight()) * 0.5f;
    if (m_selectedPass < 0) {
        float totalMs = 0.f;
        for (const auto& n : m_passes) totalMs += n.gpuMs;
        char buf[80];
        snprintf(buf, sizeof(buf), "%d passes  ·  %.2f ms GPU  ·  %d resource edges",
                 (int)m_passes.size(), totalMs, (int)m_wires.size());
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText({ p.x + (W - ts.x) * 0.5f, cy },
                    EditorColors::toU32(EditorColors::Tx1), buf);
    }
    else {
        const PassNode& sel = m_passes[m_selectedPass];
        float cx = p.x + 16.f;
        dl->AddCircleFilled({ cx + 5.f, cy + 7.f }, 5.f, EditorColors::toU32(sel.dotColor));
        cx += 14.f;

        // Pass name
        dl->AddText({ cx, cy }, EditorColors::toU32(EditorColors::Tx0), sel.name.c_str());
        cx += ImGui::CalcTextSize(sel.name.c_str()).x + 8.f;

        // Badge pill
        ImVec4 badgeCol = (sel.badge == "COMP") ? EditorColors::SRV : EditorColors::RTV;
        float bw = ImGui::CalcTextSize(sel.badge.c_str()).x + 8.f;
        dl->AddRectFilled({ cx, cy }, { cx + bw, cy + 14.f },
                          EditorColors::toU32A(badgeCol, 0.25f), 3.f);
        dl->AddText({ cx + 4.f, cy }, EditorColors::toU32(badgeCol), sel.badge.c_str());
        cx += bw + 8.f;

        // ms (colored by threshold)
        char msBuf[16]; snprintf(msBuf, sizeof(msBuf), "%.2f ms", sel.gpuMs);
        dl->AddText({ cx, cy }, EditorColors::toU32(msColor(sel.gpuMs)), msBuf);
        cx += ImGui::CalcTextSize(msBuf).x + 8.f;

        // Port counts (right side)
        int reads = 0, writes = 0;
        for (const auto& port : sel.ports) port.isOutput ? ++writes : ++reads;
        char rwBuf[32]; snprintf(rwBuf, sizeof(rwBuf), "reads %d · writes %d", reads, writes);
        ImVec2 rwSz = ImGui::CalcTextSize(rwBuf);
        dl->AddText({ p.x + W - rwSz.x - 16.f, cy },
                    EditorColors::toU32(EditorColors::Tx2), rwBuf);
    }
}

void RenderGraphPanel::drawContent() {
    const float kLegendH = 28.f;
    const float kDetailH = 28.f;
    float W = ImGui::GetWindowSize().x;
    float H = ImGui::GetWindowSize().y;
    float graphH = H - kDetailH - kLegendH;
    float totalGraphW = float(m_passes.size()) * (kNodeW + kNodeGap) - kNodeGap;

    // ---- Legend bar (resource type swatches + live totals) ----
    {
        ImGui::SetCursorPos({ 0, 0 });
        ImDrawList* leg = ImGui::GetWindowDrawList();
        ImVec2 lp  = ImGui::GetCursorScreenPos();
        ImVec2 lp2 = { lp.x + W, lp.y + kLegendH };
        leg->AddRectFilled(lp, lp2, IM_COL32(20, 20, 26, 255));
        leg->AddLine({ lp.x, lp2.y }, lp2, EditorColors::toU32(EditorColors::Line));

        float lx = lp.x + 12.f;
        float ly = lp.y + (kLegendH - ImGui::GetTextLineHeight()) * 0.5f;
        struct ResEntry { const char* label; ImVec4 color; };
        static const ResEntry kRes[] = {
            { "RTV", EditorColors::RTV }, { "SRV", EditorColors::SRV },
            { "UAV", EditorColors::UAV }, { "DSV", EditorColors::DSV },
            { "CBV", EditorColors::CBV },
        };
        for (const auto& re : kRes) {
            leg->AddRectFilled({ lx, ly + 1.f }, { lx + 9.f, ly + 9.f },
                               EditorColors::toU32(re.color), 2.f);
            lx += 12.f;
            leg->AddText({ lx, ly }, EditorColors::toU32(EditorColors::Tx2), re.label);
            lx += ImGui::CalcTextSize(re.label).x + 14.f;
        }

        // Right side: pass count + total ms
        float totalMs = 0.f;
        for (const auto& n : m_passes) totalMs += n.gpuMs;
        char rBuf[48]; snprintf(rBuf, sizeof(rBuf), "%d passes  ·  %.2f ms", (int)m_passes.size(), totalMs);
        ImVec2 rSz = ImGui::CalcTextSize(rBuf);
        leg->AddText({ lp.x + W - rSz.x - 12.f, ly }, EditorColors::toU32(EditorColors::Tx1), rBuf);

        ImGui::Dummy(ImVec2(W, kLegendH));
    }

    ImGui::BeginChild("##RGGraph", ImVec2(W, graphH), false,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl      = ImGui::GetWindowDrawList();
    ImVec2      wp      = ImGui::GetWindowPos();
    float       scrollX = ImGui::GetScrollX();

    // Radial background gradient
    {
        ImVec2 center = { wp.x + W * 0.5f - scrollX * 0.2f, wp.y + graphH * 0.4f };
        float  outerR = std::max(W, graphH) * 0.85f;
        dl->AddCircleFilled(center, outerR, IM_COL32(24, 24, 32, 255));
    }

    // graphOrigin accounts for horizontal scroll so DrawList positions track the content
    ImVec2 graphOrigin = { wp.x + 48.f - scrollX, wp.y + (graphH - kNodeH) * 0.5f };
    drawWires(dl, graphOrigin);

    // Draw nodes
    for (int i = 0; i < (int)m_passes.size(); ++i) {
        ImVec2 orig = { graphOrigin.x + i * (kNodeW + kNodeGap), graphOrigin.y };
        bool   sel  = (m_selectedPass == i);

        // Dim non-connected nodes when something selected
        if (m_selectedPass >= 0 && !sel) {
            bool connected = false;
            for (const auto& w : m_wires)
                if (w.fromPass == m_selectedPass && w.toPass == i) { connected = true; break; }
                else if (w.toPass == m_selectedPass && w.fromPass == i) { connected = true; break; }
            if (!connected) {
                dl->AddRectFilled(orig, { orig.x + kNodeW, orig.y + kNodeH },
                                  IM_COL32(0, 0, 0, 140), 6.f);
            }
        }

        drawNode(m_passes[i], orig, dl, sel);

        // Invisible button for click detection
        ImGui::SetCursorScreenPos(orig);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##node", ImVec2(kNodeW, kNodeH))) {
            m_selectedPass = (m_selectedPass == i) ? -1 : i;
        }
        ImGui::PopID();
    }

    // Scroll canvas width to fit all nodes
    ImGui::Dummy(ImVec2(totalGraphW + 96.f, graphH));
    ImGui::EndChild();

    drawDetailStrip(kLegendH + graphH, kDetailH);
}
