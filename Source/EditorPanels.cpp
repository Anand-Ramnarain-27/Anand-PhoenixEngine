#include "Globals.h"
#include "EditorPanels.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleAssets.h"
#include "ResourceCommon.h"
#include "ModuleEditor.h"
#include "SceneManager.h"
#include "CollisionSystem.h"
#include "CollisionResponse.h"
#include "UniformGridBroadPhase.h"
#include "OctreeBroadPhase.h"
#include "GameObject.h"
#include <algorithm>
#include <cctype>

// ---------------------------------------------------------------------------
// ConsolePanel
// ---------------------------------------------------------------------------

LogTag ConsolePanel::detectTag(const char* text, std::string& msgOut) {
    // Check for engine-style prefix markers: "[Editor]", "[ok]", "[warn]", etc.
    if (!text) { msgOut = ""; return LogTag::Info; }

    // Colour-based tag inference used when add() passes EditorColors
    // is done by the caller via the color argument — see add().
    // Here we detect textual [tag] prefixes.
    if (strncmp(text, "[ok]",    4) == 0 || strncmp(text, "[OK]",    4) == 0)
    { msgOut = text + 4; if (!msgOut.empty() && msgOut[0] == ' ') msgOut = msgOut.substr(1); return LogTag::Ok; }
    if (strncmp(text, "[warn]",  6) == 0 || strncmp(text, "[WARN]",  6) == 0)
    { msgOut = text + 6; if (!msgOut.empty() && msgOut[0] == ' ') msgOut = msgOut.substr(1); return LogTag::Warn; }
    if (strncmp(text, "[error]", 7) == 0 || strncmp(text, "[ERROR]", 7) == 0)
    { msgOut = text + 7; if (!msgOut.empty() && msgOut[0] == ' ') msgOut = msgOut.substr(1); return LogTag::Error; }
    if (strncmp(text, "[info]",  6) == 0 || strncmp(text, "[INFO]",  6) == 0)
    { msgOut = text + 6; if (!msgOut.empty() && msgOut[0] == ' ') msgOut = msgOut.substr(1); return LogTag::Info; }
    msgOut = text;
    return LogTag::Info;
}

void ConsolePanel::add(const char* text, const ImVec4& color) {
    ConsoleEntry e;
    e.text  = text ? text : "";
    e.color = color;
    e.tag   = detectTag(text, e.message);

    // Infer tag from color when no textual prefix was found
    if (e.tag == LogTag::Info && e.message == e.text) {
        if (color.x < 0.5f && color.y > 0.6f && color.z < 0.5f)      e.tag = LogTag::Ok;
        else if (color.x > 0.6f && color.y > 0.6f && color.z < 0.3f) e.tag = LogTag::Warn;
        else if (color.x > 0.6f && color.y < 0.4f && color.z < 0.4f) e.tag = LogTag::Error;
    }
    m_entries.push_back(std::move(e));
}

void ConsolePanel::drawContent() {
    // ---- Toolbar ----
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  ImVec2(4, 4));

    if (ImGui::SmallButton("Clear")) clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);
    ImGui::SameLine(0, 12.f);
    ImGui::SetNextItemWidth(160.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,     EditorColors::toU32(EditorColors::Bg0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, EditorColors::toU32(EditorColors::Bg2));
    ImGui::InputTextWithHint("##filter", "Filter...", m_filterBuf, sizeof(m_filterBuf));
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    ImGui::Separator();

    // ---- Log area ----
    const float kCmdH   = 28.f;
    const float logH    = ImGui::GetContentRegionAvail().y - kCmdH - 4.f;
    ImGui::BeginChild("##log", ImVec2(0, logH > 20.f ? logH : 20.f), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    bool hasFilter = m_filterBuf[0] != '\0';
    std::string filterLow = m_filterBuf;
    std::transform(filterLow.begin(), filterLow.end(), filterLow.begin(), ::tolower);

    for (const auto& e : m_entries) {
        // Filter
        if (hasFilter) {
            std::string textLow = e.text;
            std::transform(textLow.begin(), textLow.end(), textLow.begin(), ::tolower);
            if (textLow.find(filterLow) == std::string::npos) continue;
        }

        // Tag pill colors
        ImVec4 tagFg, tagBg;
        const char* tagLabel;
        switch (e.tag) {
        case LogTag::Ok:
            tagFg = EditorColors::Ok;
            tagBg = ImVec4(70.f/255.f, 207.f/255.f, 139.f/255.f, 0.12f);
            tagLabel = "ok"; break;
        case LogTag::Warn:
            tagFg = EditorColors::Warn;
            tagBg = ImVec4(232.f/255.f, 193.f/255.f, 74.f/255.f, 0.12f);
            tagLabel = "warn"; break;
        case LogTag::Error:
            tagFg = EditorColors::Crit;
            tagBg = ImVec4(232.f/255.f, 96.f/255.f, 110.f/255.f, 0.12f);
            tagLabel = "error"; break;
        default:
            tagFg = EditorColors::SRV;
            tagBg = ImVec4(74.f/255.f, 168.f/255.f, 232.f/255.f, 0.12f);
            tagLabel = "info"; break;
        }

        // Draw tag pill via DrawList (3px corner radius, 1+4px padding)
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetCursorScreenPos();
        const float pillH   = ImGui::GetTextLineHeight();
        ImVec2      tagSz   = ImGui::CalcTextSize(tagLabel);
        const float pillW   = tagSz.x + 8.f;

        dl->AddRectFilled(pos, { pos.x + pillW, pos.y + pillH },
                          ImGui::ColorConvertFloat4ToU32(tagBg), 3.f);
        dl->AddText({ pos.x + 4.f, pos.y }, ImGui::ColorConvertFloat4ToU32(tagFg), tagLabel);

        // Advance cursor past pill, then print message
        ImGui::Dummy(ImVec2(pillW + 6.f, pillH));
        ImGui::SameLine(0, 0);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
        ImGui::TextUnformatted(e.message.empty() ? e.text.c_str() : e.message.c_str());
        ImGui::PopStyleColor();
    }

    if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.f)
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    // ---- Command input ----
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_ChildBg,  EditorColors::toU32(ImVec4(0.043f,0.043f,0.051f,1.f)));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,  EditorColors::toU32(ImVec4(0.043f,0.043f,0.051f,1.f)));
    ImGui::BeginChild("##cmdbar", ImVec2(0, kCmdH), false);
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Accent);
    ImGui::TextUnformatted(">");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 4.f);
    ImGui::SetNextItemWidth(-1.f);
    bool entered = ImGui::InputText("##cmd", m_cmdBuf, sizeof(m_cmdBuf),
                                    ImGuiInputTextFlags_EnterReturnsTrue);
    if (entered && m_cmdBuf[0] != '\0') {
        std::string cmd = m_cmdBuf;
        add(cmd.c_str(), EditorColors::Tx1);
        // TODO: dispatch cmd to engine command handler
        m_cmdBuf[0] = '\0';
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

void PerformancePanel::drawContent() {
    ImGui::Text("FPS:  %.1f", app->getFPS());
    ImGui::Text("CPU:  %.2f ms", app->getAvgElapsedMs());
    if (m_gpuReady) ImGui::Text("GPU:  %.2f ms", m_gpuMs);
    ImGui::Separator();
    ImGui::Text("VRAM: %llu MB", m_gpuMem);
    ImGui::Text("RAM:  %llu MB", m_ramMem);
    ImGui::Separator();
    float ordered[kHistory];
    float maxFPS = 60.0f;
    for (int i = 0; i < kHistory; ++i) {
        ordered[i] = m_fpsHistory[(m_fpsIdx + i) % kHistory];
        if (ordered[i] > maxFPS) maxFPS = ordered[i];
    }
    ImGui::PlotLines("##fps", ordered, kHistory, 0, nullptr, 0, maxFPS * 1.1f, ImVec2(-1, 80));
}

void ResourcesPanel::drawContent() {
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

ImVec4 ResourcesPanel::typeColor(ResourceBase::Type t) {
    switch (t) {
    case ResourceBase::Type::Mesh: return { 0.6f, 0.9f, 1.0f, 1.f };
    case ResourceBase::Type::Material: return { 1.0f, 0.85f, 0.5f, 1.f };
    case ResourceBase::Type::Texture: return { 0.8f, 0.6f, 1.0f, 1.f };
    default: return { 0.6f, 1.f, 0.6f, 1.f };
    }
}

void CollisionDebugPanel::drawContent() {
    CollisionSystem* cs = m_editor->getCollisionSystem();
    if (!cs) { textMuted("No collision system."); return; }

    const CollisionResults& r = cs->getResults();

    // ---- Debug draw toggles (synced with Debug menu) ----------------------
    ImGui::SeparatorText("Debug Draw");
    auto* sm = m_editor->getSceneManager();
    if (sm) {
        auto& s = sm->getSettings();
        ImGui::Checkbox("AABB Bounding Volumes",    &s.debugDrawBounds);
        ImGui::Checkbox("Camera Frustum",           &s.debugDrawCameraFrustums);
        ImGui::Checkbox("Contact Points & Normals", &s.debugDrawContacts);
        ImGui::Checkbox("Broadphase Grid",          &s.debugDrawGrid);
    }
    ImGui::Spacing();

    // ---- Broadphase mode radio buttons -----------------------------------
    ImGui::SeparatorText("Broadphase Mode");
    {
        bool isBrute  = !cs->isUsingGrid() && !cs->isUsingOctree();
        bool isGrid   =  cs->isUsingGrid();
        bool isOctree =  cs->isUsingOctree();

        if (ImGui::RadioButton("Brute Force##bp",   isBrute))  cs->useBruteForceBroadPhase();
        ImGui::SameLine(); ImGui::TextDisabled("O(n²)");
        if (ImGui::RadioButton("Uniform Grid##bp",  isGrid))   cs->useGridBroadPhase();
        ImGui::SameLine(); ImGui::TextDisabled("O(n log n)");
        if (ImGui::RadioButton("Octree##bp",        isOctree)) cs->useOctreeBroadPhase();
        ImGui::SameLine(); ImGui::TextDisabled("adaptive");
    }

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

    // ---- Pipeline statistics table ----------------------------------------
    ImGui::SeparatorText("Pipeline  (this frame)");

    if (ImGui::BeginTable("##colTimings", 4,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Phase",   ImGuiTableColumnFlags_WidthFixed, 96.f);
        ImGui::TableSetupColumn("Pairs",   ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn("CPU ms",  ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("Type",    ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto msColor = [](float ms) -> ImVec4 {
            if (ms > 2.f) return EditorColors::Crit; // #e8606e
            if (ms > 1.f) return EditorColors::Hot;  // #e8924a
            return EditorColors::Ok;                  // #46cf8b
        };

        // Broad
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Broad");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%u", r.broadCount);
        ImGui::TableSetColumnIndex(2);
        ImGui::PushStyleColor(ImGuiCol_Text, msColor(r.broadPhaseMs));
        ImGui::Text("%.3f", r.broadPhaseMs);
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(3); ImGui::TextDisabled("%s", cs->getBroadPhaseName());

        // Mid
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Mid");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%u", r.midCount);
        ImGui::TableSetColumnIndex(2);
        ImGui::PushStyleColor(ImGuiCol_Text, msColor(r.midPhaseMs));
        ImGui::Text("%.3f", r.midPhaseMs);
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(3); ImGui::TextDisabled("OBB-SAT");

        // Narrow
        bool anyHit = r.narrowCount > 0;
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Narrow");
        ImGui::TableSetColumnIndex(1);
        if (anyHit) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.35f, 0.35f, 1.f));
        ImGui::Text("%u", r.narrowCount);
        if (anyHit) ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(2);
        ImGui::PushStyleColor(ImGuiCol_Text, msColor(r.narrowPhaseMs));
        ImGui::Text("%.3f", r.narrowPhaseMs);
        ImGui::PopStyleColor();
        ImGui::TableSetColumnIndex(3); ImGui::TextDisabled("GJK/SAT");

        // Total
        float totalMs = r.broadPhaseMs + r.midPhaseMs + r.narrowPhaseMs;
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx1);
        ImGui::TextUnformatted("Total");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%u", r.broadCount + r.midCount + r.narrowCount);
        ImGui::TableSetColumnIndex(2);
        ImGui::PushStyleColor(ImGuiCol_Text, msColor(totalMs));
        ImGui::Text("%.3f", totalMs);
        ImGui::PopStyleColor(2);
        ImGui::TableSetColumnIndex(3);

        ImGui::EndTable();
    }

    // Extra grid/octree info below table
    if (cs->isUsingGrid()) {
        ImGui::TextDisabled("  grid: %d occupied cell(s)", cs->getLastGridCellCount());
    } else if (cs->isUsingOctree()) {
        ImGui::TextDisabled("  octree: %d node(s), %d leaf(ves)",
            cs->getLastOctreeNodeCount(), cs->getLastOctreeLeafCount());
    }

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
            ImGui::Text("  point   (%.2f, %.2f, %.2f)", c.point.x,  c.point.y,  c.point.z);
            ImGui::Text("  normal  (%.2f, %.2f, %.2f)", c.normal.x, c.normal.y, c.normal.z);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    // ---- Narrow phase stats (from CollisionResults) ----------------------
    ImGui::SeparatorText("Narrow Phase Stats");
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::Text("Pairs tested"); ImGui::PopStyleColor();
    ImGui::SameLine(150.f);
    ImGui::Text("%u", r.narrowCount);

    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::Text("Contacts found"); ImGui::PopStyleColor();
    ImGui::SameLine(150.f);
    ImGui::Text("%u", (uint32_t)r.contacts.size());

    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::Text("Tunneling prev."); ImGui::PopStyleColor();
    ImGui::SameLine(150.f);
    ImGui::TextDisabled("0");  // swept AABB handles it at broad phase level

    // ---- Collision Response tuning ----------------------------------------
    CollisionResponse* cr = m_editor->getCollisionResponse();
    if (cr) {
        ImGui::SeparatorText("Collision Response");
        ImGui::SetNextItemWidth(200.f);
        ImGui::SliderFloat("Restitution Bias##crp", &cr->correctionPercent, 0.f, 1.f, "%.3f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Fraction of penetration corrected per frame.\n"
                              "0 = no position correction, 1 = full correction (may jitter).");
        ImGui::SetNextItemWidth(200.f);
        ImGui::DragFloat("Penetration Slop##crslop", &cr->correctionSlop, 0.0001f, 0.f, 0.1f, "%.4f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Minimum penetration depth before correction kicks in.\n"
                              "Small positive value eliminates resting-contact jitter.");
    }
}

const char* ResourcesPanel::typeName(ResourceBase::Type t) {
    switch (t) {
    case ResourceBase::Type::Mesh: return "Mesh";
    case ResourceBase::Type::Texture: return "Texture";
    case ResourceBase::Type::Material: return "Material";
    case ResourceBase::Type::Model: return "Model";
    case ResourceBase::Type::Scene: return "Scene";
    default: return "Unknown";
    }
}
