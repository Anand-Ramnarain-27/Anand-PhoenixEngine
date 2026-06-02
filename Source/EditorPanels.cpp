#include "Globals.h"
#include "EditorPanels.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleAssets.h"
#include "ResourceCommon.h"
#include "ModuleEditor.h"
#include "CollisionSystem.h"
#include "UniformGridBroadPhase.h"
#include "OctreeBroadPhase.h"
#include "GameObject.h"

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

    // ---- Broad phase selector & tuning -----------------------------------
    ImGui::SeparatorText("Broad Phase");
    ImGui::Text("Active: %s", cs->getBroadPhaseName());
    ImGui::Spacing();

    if (ImGui::Button("Brute Force"))  cs->useBruteForceBroadPhase();
    ImGui::SameLine();
    if (ImGui::Button("Uniform Grid")) cs->useGridBroadPhase();
    ImGui::SameLine();
    if (ImGui::Button("Octree"))       cs->useOctreeBroadPhase();

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
                     ms < 2.f  ? ImVec4(1.f,0.85f,0.2f,1.f) :
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
            ImGui::Text("  point   (%.2f, %.2f, %.2f)", c.point.x,  c.point.y,  c.point.z);
            ImGui::Text("  normal  (%.2f, %.2f, %.2f)", c.normal.x, c.normal.y, c.normal.z);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
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
