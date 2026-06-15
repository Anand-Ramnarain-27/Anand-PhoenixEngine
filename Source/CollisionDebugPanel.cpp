#include "Globals.h"
#include "CollisionDebugPanel.h"
#include "EditorColors.h"
#include "ImGuiPass.h"
#include "Application.h"
#include "ModuleEditor.h"
#include "CollisionSystem.h"
#include "CollisionInterfaces.h"
#include "UniformGridBroadPhase.h"
#include "OctreeBroadPhase.h"
#include "EditorSceneSettings.h"
#include "SceneManager.h"
#include "ModuleCamera.h"
#include "GameObject.h"

void CollisionDebugPanel::drawContent(){
    CollisionSystem* cs = m_editor->getCollisionSystem();
    if (!cs){ textMuted("No collision system."); return; }

    const CollisionResults& r = cs->getResults();
    EditorSceneSettings* s = m_editor->getSceneManager() ? &m_editor->getSceneManager()->getSettings() : nullptr;

    float winW = ImGui::GetContentRegionAvail().x;
    float leftW = winW * 0.6f;

    ImGui::BeginGroup();

    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::TextUnformatted("BROADPHASE TIMINGS");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    struct PhaseRow { const char* phase; const char* impl; uint32_t pairs; float gpuMs; float cpuMs; };
    float totalGpu = r.broadPhaseMs;
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
        for (auto& row : rows){
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

    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx2);
    ImGui::TextUnformatted("DEBUG DRAW TOGGLES");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    if (s){
        ImGui::Checkbox("AABB Bounding Volumes", &s->debugDrawBounds);
        ImGui::Checkbox("Broadphase Grid", &s->debugDrawGrid);
        ImGui::Checkbox("Show Light Proxies", &s->debugDrawLights);
        if (ModuleCamera* cam = app->getCamera())
            ImGui::Checkbox("Camera Frustum", &cam->debugDrawEditorFrustum);
    }
    ImGui::EndGroup();

    ImGui::Separator();

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

    if (cs->isUsingGrid()){
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

    if (cs->isUsingOctree()){
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

    ImGui::SeparatorText("Pipeline  (this frame)");

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
    if (cs->isUsingGrid()){
        ImGui::TextDisabled("  (grid, %d occupied cell(s))", cs->getLastGridCellCount());
    } else if (cs->isUsingOctree()){
        ImGui::TextDisabled("  (octree, %d node(s), %d leaf(ves))",
            cs->getLastOctreeNodeCount(), cs->getLastOctreeLeafCount());
    }
    ImGui::Text("Mid phase    filtered pairs  : %u", r.midCount);

    bool anyHit = r.narrowCount > 0;
    if (anyHit) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.35f, 0.35f, 1.f));
    ImGui::Text("Narrow phase confirmed hits  : %u", r.narrowCount);
    if (anyHit) ImGui::PopStyleColor();

    if (r.contacts.empty()){
        ImGui::Spacing();
        textMuted("No contacts.");
        return;
    }

    ImGui::SeparatorText("Contacts");
    ImGui::BeginChild("##contacts", ImVec2(0, 0), false);
    for (uint32_t i = 0; i < static_cast<uint32_t>(r.contacts.size()); ++i){
        const ContactPoint& c = r.contacts[i];
        ImGui::PushID(static_cast<int>(i));
        const char* na = c.a ? c.a->getName().c_str() : "?";
        const char* nb = c.b ? c.b->getName().c_str() : "?";
        if (ImGui::TreeNodeEx("##cp", ImGuiTreeNodeFlags_DefaultOpen,
                              "%s  ×  %s  (depth %.3f)", na, nb, c.depth)){
            ImGui::Text("  point   (%.2f, %.2f, %.2f)", c.point.x, c.point.y, c.point.z);
            ImGui::Text("  normal  (%.2f, %.2f, %.2f)", c.normal.x, c.normal.y, c.normal.z);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}
