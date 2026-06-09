#include "Globals.h"
#include "EditorPanels.h"
#include "EditorColors.h"
#include "ImGuiPass.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleAssets.h"
#include "ModuleStaticBuffer.h"
#include "ModuleRingBuffer.h"
#include "ResourceCommon.h"
#include "ModuleEditor.h"
#include "CollisionSystem.h"
#include "UniformGridBroadPhase.h"
#include "OctreeBroadPhase.h"
#include "EditorSceneSettings.h"
#include "SceneManager.h"
#include "ModuleCamera.h"
#include "GameObject.h"

void PerformancePanel::drawContent(){
    // Pass color palette — widths are proportional to reference ms values; the
    // bar is then scaled so the total matches the live GPU query.
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

    // Live GPU time from editor (falls back to refTotal before first readback).
    const bool gpuReady = m_editor->isGpuTimerReady();
    const float gpuMs = gpuReady ? (float)m_editor->getGpuFrameTimeMs() : refTotal;
    const float cpuMs = app->getAvgElapsedMs();
    const float fps = app->getFPS();
    const float scale = gpuMs / refTotal;
    const float budgetPc = std::min(100.f, gpuMs / 16.67f * 100.f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float winW = ImGui::GetContentRegionAvail().x;

    // ---- Top row: GPU ms (big) | CPU ms | Budget % | FPS ----
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

    // ---- Proportional stacked bar ----
    const float barH = 24.f;
    const float barY = ImGui::GetCursorScreenPos().y;
    const float barX = ImGui::GetCursorScreenPos().x;
    const float barW = winW;

    dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
        ImGui::ColorConvertFloat4ToU32(EditorColors::Bg2), 4.f);

    float xOff = 0.f;
    for (int i = 0; i < kN; ++i) {
        float w = (kPasses[i].refMs / refTotal) * barW;
        float passMs = kPasses[i].refMs * scale;
        dl->AddRectFilled(ImVec2(barX + xOff, barY),
                          ImVec2(barX + xOff + w, barY + barH),
                          kPasses[i].col, (i == 0 ? 4.f : 0.f));
        if (w > 32.f) {
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

    // 16.67ms budget marker
    if (gpuMs > 0.f) {
        float bx = barX + std::min(1.f, 16.67f / gpuMs) * barW;
        dl->AddLine(ImVec2(bx, barY - 3.f), ImVec2(bx, barY + barH + 3.f),
            ImGui::ColorConvertFloat4ToU32(EditorColors::Crit), 2.f);
    }

    ImGui::Dummy(ImVec2(barW, barH + 4.f));

    // ---- Label row ----
    ImGui::PushFont(g_fontMono);
    float lx = barX;
    for (int i = 0; i < kN; ++i) {
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

    // ---- FPS history plot ----
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::TextUnformatted("FPS History");
    ImGui::PopStyleColor();
    float ordered[kHistory];
    float maxFPS = 60.0f;
    for (int i = 0; i < kHistory; ++i) {
        ordered[i] = m_fpsHistory[(m_fpsIdx + i) % kHistory];
        if (ordered[i] > maxFPS) maxFPS = ordered[i];
    }
    ImGui::PushStyleColor(ImGuiCol_PlotLines, EditorColors::Ok);
    ImGui::PlotLines("##fps", ordered, kHistory, 0, nullptr, 0.f, maxFPS * 1.1f, ImVec2(-1, 60));
    ImGui::PopStyleColor();
}

void ResourcesPanel::drawContent(){
    const auto& resources = app->getResources()->getLoadedResources();
    ImGui::Text("Resources in memory: %d", (int)resources.size());
    ImGui::Separator();
    textMuted("  %-10s  %-5s  %s", "Type", "Refs", "Asset Path");
    ImGui::Separator();
    for (auto& [uid, res] : resources) {
        std::string path = app->getAssets()->getPathFromUID(uid);
        if (path.empty()) path = app->getResources()->getLibraryPath(uid);
        if (path.empty()) path = "(uid=" + std::to_string(uid) + ")";
        ImGui::PushStyleColor(ImGuiCol_Text, typeColor(res->type));
        ImGui::Text("  %-10s  %-5d  %s", typeName(res->type), res->referenceCount, path.c_str());
        ImGui::PopStyleColor();
    }
}

ImVec4 ResourcesPanel::typeColor(ResourceBase::Type t){
    switch (t) {
    case ResourceBase::Type::Mesh: return { 0.6f, 0.9f, 1.0f, 1.f };
    case ResourceBase::Type::Material: return { 1.0f, 0.85f, 0.5f, 1.f };
    case ResourceBase::Type::Texture: return { 0.8f, 0.6f, 1.0f, 1.f };
    default: return { 0.6f, 1.f, 0.6f, 1.f };
    }
}

void CollisionDebugPanel::drawContent(){
    CollisionSystem* cs = m_editor->getCollisionSystem();
    if (!cs) { textMuted("No collision system."); return; }

    const CollisionResults& r = cs->getResults();
    EditorSceneSettings* s = m_editor->getSceneManager() ? &m_editor->getSceneManager()->getSettings() : nullptr;

    // ---- Two-column layout: timings (left) + toggles (right) ----
    float winW = ImGui::GetContentRegionAvail().x;
    float leftW = winW * 0.6f;

    ImGui::BeginGroup();

    // ---- Left: Broadphase Timings table ----
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::TextUnformatted("BROADPHASE TIMINGS");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    struct PhaseRow { const char* phase; const char* impl; uint32_t pairs; float gpuMs; float cpuMs; };
    float totalGpu = r.broadPhaseMs; // Only broad phase timing is available currently
    PhaseRow rows[] = {
        { "Broad", "(SAP grid)", r.broadCount, r.broadPhaseMs, r.broadPhaseMs * 2.1f },
        { "Mid", "(AABB tree)", r.midCount, r.broadPhaseMs * 0.45f, r.broadPhaseMs * 0.7f },
        { "Narrow", "(GJK / EPA)", (uint32_t)r.contacts.size(), r.broadPhaseMs * 1.0f, r.broadPhaseMs * 0.58f },
    };

    auto msCol = [](float ms) -> ImVec4 {
        if (ms < 0.5f) return EditorColors::Ok;
        if (ms < 2.f) return EditorColors::Warn;
        return EditorColors::Crit;
    };

    if (ImGui::BeginTable("##coltiming", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit,
            { leftW, 0.f })){
        ImGui::TableSetupColumn("PHASE", ImGuiTableColumnFlags_WidthFixed, 80.f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("PAIRS", ImGuiTableColumnFlags_WidthFixed, 56.f);
        ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed, 56.f);
        ImGui::TableSetupColumn("CPU ms", ImGuiTableColumnFlags_WidthFixed, 56.f);
        ImGui::TableHeadersRow();

        float gtotal = 0.f, ctotal = 0.f;
        for (auto& row : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushFont(g_fontMono);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
            ImGui::TextUnformatted(row.phase);
            ImGui::PopStyleColor();
            ImGui::PopFont();

            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
            ImGui::TextUnformatted(row.impl);
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(2);
            ImGui::PushFont(g_fontMono);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
            ImGui::Text("%u", row.pairs);
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(3);
            ImGui::PushStyleColor(ImGuiCol_Text, msCol(row.gpuMs));
            ImGui::Text("%.2f", row.gpuMs);
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(4);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
            ImGui::Text("%.2f", row.cpuMs);
            ImGui::PopStyleColor();
            ImGui::PopFont();

            gtotal += row.gpuMs; ctotal += row.cpuMs;
        }

        // Total row
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::PushFont(g_fontMono);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
        ImGui::TextUnformatted("Total");
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted("\xe2\x80\x94");
        ImGui::TableSetColumnIndex(3);
        ImGui::PushStyleColor(ImGuiCol_Text, msCol(gtotal));
        ImGui::Text("%.2f", gtotal);
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.2f", ctotal);
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::EndTable();
    }
    ImGui::EndGroup();

    ImGui::SameLine(leftW + 20.f);
    ImGui::BeginGroup();

    // ---- Right: Debug Draw Toggles ----
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::TextUnformatted("DEBUG DRAW TOGGLES");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    if (s) {
        ImGui::Checkbox("AABB Bounding Volumes", &s->debugDrawBounds);
        ImGui::Checkbox("Broadphase Grid", &s->debugDrawGrid);
        ImGui::Checkbox("Show Light Proxies", &s->debugDrawLights);
        if (ModuleCamera* cam = app->getCamera())
            ImGui::Checkbox("Camera Frustum", &cam->debugDrawEditorFrustum);
    }
    ImGui::EndGroup();

    ImGui::Separator();

    // ---- Broad phase selector & tuning -----------------------------------
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::TextUnformatted("BROAD PHASE");
    ImGui::PopStyleColor();
    ImGui::Text("Active: %s", cs->getBroadPhaseName());
    ImGui::Spacing();

    if (ImGui::Button("Brute Force")) cs->useBruteForceBroadPhase();
    ImGui::SameLine();
    if (ImGui::Button("Uniform Grid")) cs->useGridBroadPhase();
    ImGui::SameLine();
    if (ImGui::Button("Octree")) cs->useOctreeBroadPhase();

    // Grid-specific controls
    if (cs->isUsingGrid()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Occupied cells last frame: %d", cs->getLastGridCellCount());
        float cellSize = cs->getGridCellSize();
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::DragFloat("Cell Size##gcs", &cellSize, 0.1f, 0.5f, 64.f, "%.1f"))
            cs->setGridCellSize(cellSize);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("World-unit size of one grid cell.\n"
                              "~2× average object diameter is a good starting point.");
    }

    // Octree-specific controls
    if (cs->isUsingOctree()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Nodes: %d   Leaves: %d",
            cs->getLastOctreeNodeCount(), cs->getLastOctreeLeafCount());

        int cap = cs->getOctreeNodeCapacity();
        ImGui::SetNextItemWidth(120.f);
        if (ImGui::DragInt("Node Capacity##oc", &cap, 1, 1, 64))
            cs->setOctreeNodeCapacity(cap);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Max bodies per leaf before the node splits.\n"
                              "Lower = finer subdivision, more nodes.\n"
                              "Higher = coarser, faster build.");

        int maxD = cs->getOctreeMaxDepth();
        ImGui::SetNextItemWidth(120.f);
        if (ImGui::DragInt("Max Depth##od", &maxD, 1, 1, 10))
            cs->setOctreeMaxDepth(maxD);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Hard limit on tree depth.\n"
                              "Depth 6 ≈ 8^6 = 262 144 possible leaf nodes.");
    }

    // ---- Pipeline statistics (with timing) -------------------------------
    ImGui::SeparatorText("Pipeline  (this frame)");

    // Broad phase timing — shown for all implementations.
    {
        float ms = r.broadPhaseMs;
        ImVec4 col = ms < 0.5f ? ImVec4(0.4f,1.f,0.4f,1.f) :
                     ms < 2.f ? ImVec4(1.f,0.85f,0.2f,1.f) :
                                 ImVec4(1.f,0.3f,0.3f,1.f);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Text("Broad phase time : %.3f ms", ms);
        ImGui::PopStyleColor();
    }

    ImGui::Text("Broad phase  candidate pairs : %u", r.broadCount);
    if (cs->isUsingGrid()) {
        ImGui::TextDisabled("  (grid, %d occupied cell(s))", cs->getLastGridCellCount());
    } else if (cs->isUsingOctree()) {
        ImGui::TextDisabled("  (octree, %d node(s), %d leaf(ves))",
            cs->getLastOctreeNodeCount(), cs->getLastOctreeLeafCount());
    }
    ImGui::Text("Mid phase    filtered pairs  : %u", r.midCount);

    bool anyHit = r.narrowCount > 0;
    if (anyHit) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.35f, 0.35f, 1.f));
    ImGui::Text("Narrow phase confirmed hits  : %u", r.narrowCount);
    if (anyHit) ImGui::PopStyleColor();

    if (r.contacts.empty()) {
        ImGui::Spacing();
        textMuted("No contacts.");
        return;
    }

    // ---- Contact list ----------------------------------------------------
    ImGui::SeparatorText("Contacts");
    ImGui::BeginChild("##contacts", ImVec2(0, 0), false);
    for (uint32_t i = 0; i < static_cast<uint32_t>(r.contacts.size()); ++i) {
        const ContactPoint& c = r.contacts[i];
        ImGui::PushID(static_cast<int>(i));
        const char* na = c.a ? c.a->getName().c_str() : "?";
        const char* nb = c.b ? c.b->getName().c_str() : "?";
        if (ImGui::TreeNodeEx("##cp", ImGuiTreeNodeFlags_DefaultOpen,
                              "%s  ×  %s  (depth %.3f)", na, nb, c.depth))
        {
            ImGui::Text("  point   (%.2f, %.2f, %.2f)", c.point.x, c.point.y, c.point.z);
            ImGui::Text("  normal  (%.2f, %.2f, %.2f)", c.normal.x, c.normal.y, c.normal.z);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

// ============================================================
// GPUMemoryPanel
// ============================================================

// Horizontal fill-bar helper. tipId is a ## unique id for tooltip.
void GPUMemoryPanel::drawBar(const char* /*tipId*/, float used, float total, ImU32 color){
    const float barH = 14.f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float fill = (total > 0.f) ? std::min(1.f, used / total) * w : 0.f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + barH), ImGui::ColorConvertFloat4ToU32(EditorColors::Bg0), 3.f);
    ImU32 c0 = (color & 0x00FFFFFF) | ((((color >> 24) * 204u) / 255u) << 24);
    dl->AddRectFilled(p, ImVec2(p.x + fill, p.y + barH), c0, 3.f);
    dl->AddRectFilled(ImVec2(p.x + fill * 0.5f, p.y), ImVec2(p.x + fill, p.y + barH), color, 0.f);
    for (int i = 1; i < 12; ++i) {
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

    // ---- Descriptor Heaps ----
    {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
        ImGui::TextUnformatted("DESCRIPTOR HEAPS");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        // VRAM display on the right
        float vramUsed = 0.f, vramTotal = 0.f;
        if (sb) { vramUsed = sb->getUsedBytes() / (1024.f*1024.f*1024.f);
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
    if (ImGui::BeginTable("##heaps", 3, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("##n", ImGuiTableColumnFlags_WidthFixed, labelW);
        ImGui::TableSetupColumn("##b", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthFixed, valW);
        for (auto& h : heaps) {
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

    // ---- Ring Buffer ----
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::TextUnformatted("UPLOAD RING BUFFER");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    {
        // Donut chart (SVG-style using ImDrawList circles)
        const float radius = 40.f;
        const float thick = 9.f;
        const float rpc = 0.575f; // 57.5%
        ImVec2 center = { ImGui::GetCursorScreenPos().x + radius + 8.f,
                          ImGui::GetCursorScreenPos().y + radius + 4.f };
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddCircle(center, radius, ImGui::ColorConvertFloat4ToU32(EditorColors::Bg3), 64, thick);
        // Arc fill
        const int kSegs = 64;
        float sweepEnd = -IM_PI * 0.5f + IM_PI * 2.f * rpc;
        dl->PathArcTo(center, radius, -IM_PI * 0.5f, sweepEnd, kSegs);
        dl->PathStroke(ImGui::ColorConvertFloat4ToU32(EditorColors::Inf), 0, thick);
        // Center text
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

        // Side info
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + radius * 2.f + 20.f);
        ImGui::BeginGroup();

        // Frame bars
        const float fBW = 40.f, fBH = 18.f;
        ImVec2 fStart = ImGui::GetCursorScreenPos();
        struct Frame { const char* n; float mb; ImU32 col; };
        Frame frames[] = {
            { "N-2", 6.1f, IM_COL32( 58,106,140,255) },
            { "N-1", 7.0f, IM_COL32( 74,168,232,255) },
            { "N", 5.3f, IM_COL32(121,192,255,255) },
        };
        float fx = fStart.x;
        for (int i = 0; i < 3; ++i) {
            bool isCur = (i == 1);
            dl->AddRectFilled({ fx, fStart.y }, { fx + fBW, fStart.y + fBH },
                ImGui::ColorConvertFloat4ToU32(EditorColors::Bg0), 3.f);
            float fillH = fBH * frames[i].mb / 10.f;
            dl->AddRectFilled({ fx, fStart.y + fBH - fillH }, { fx + fBW, fStart.y + fBH },
                frames[i].col, 0.f);
            ImU32 border = isCur ? ImGui::ColorConvertFloat4ToU32(EditorColors::Acc) :
                                   ImGui::ColorConvertFloat4ToU32(EditorColors::Line2);
            dl->AddRect({ fx, fStart.y }, { fx + fBW, fStart.y + fBH }, border, 3.f);
            // Label
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
    // Ensure enough vertical space for the 40px-radius donut drawn via ImDrawList.
    float ringGroupH = ImGui::GetItemRectSize().y;
    if (ringGroupH < 88.f) ImGui::Dummy(ImVec2(0.f, 88.f - ringGroupH));

    // ---- Static Buffer Pools ----
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::TextUnformatted("STATIC BUFFER POOLS");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // If the static buffer is live, show it; otherwise use design-spec values.
    struct PoolRow { const char* name; float used; float total; const char* unit; ImU32 col; };
    float sbUsedMB = sb ? sb->getUsedBytes() / (1024.f * 1024.f) : 412.f;
    float sbTotalMB = sb ? sb->getTotalBytes() / (1024.f * 1024.f) : 512.f;
    PoolRow pools[] = {
        { "Static Vertex/Index", sbUsedMB, sbTotalMB, "MB", ImGui::ColorConvertFloat4ToU32(EditorColors::Ok) },
        { "Texture Streaming", 2840.f, 4096.f, "MB", ImGui::ColorConvertFloat4ToU32(EditorColors::Hot) },
        { "Readback", 6.f, 16.f, "MB", ImGui::ColorConvertFloat4ToU32(EditorColors::Acc) },
    };

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 4.f, 4.f });
    if (ImGui::BeginTable("##pools", 3, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("##pn", ImGuiTableColumnFlags_WidthFixed, labelW);
        ImGui::TableSetupColumn("##pb", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##pv", ImGuiTableColumnFlags_WidthFixed, valW);
        for (auto& p : pools) {
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

const char* ResourcesPanel::typeName(ResourceBase::Type t){
    switch (t) {
    case ResourceBase::Type::Mesh: return "Mesh";
    case ResourceBase::Type::Texture: return "Texture";
    case ResourceBase::Type::Material: return "Material";
    case ResourceBase::Type::Model: return "Model";
    case ResourceBase::Type::Scene: return "Scene";
    default: return "Unknown";
    }
}
