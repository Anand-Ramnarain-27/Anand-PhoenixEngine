#include "Globals.h"
#include "SceneSettingsPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "SceneManager.h"
#include "EditorSceneSettings.h"
#include "EnvironmentSystem.h"
#include "CollisionSystem.h"
#include "ComponentRigidbody.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

static bool accentHeader(const char* label, const ImVec4& borderColor) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  h = ImGui::GetFrameHeight();
    bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::GetWindowDrawList()->AddRectFilled(
        p, { p.x + 3.f, p.y + h }, ImGui::ColorConvertFloat4ToU32(borderColor), 1.f);
    return open;
}

void SceneSettingsPanel::drawContent() {
    if (!m_editor->getSceneManager()) { textMuted("No scene manager."); return; }
    EditorSceneSettings& s = m_editor->getSceneManager()->getSettings();

    // ================================================================
    // 1. ENVIRONMENT — skybox asset picker
    // ================================================================
    if (accentHeader("Environment", EditorColors::SRV)) {
        auto& sky = s.skybox;
        ImGui::Checkbox("Enable Skybox", &sky.enabled);
        ImGui::Separator();

        if (!m_scanned || ImGui::Button("Refresh##sky")) {
            m_skyboxFiles.clear(); m_selectedSkybox = -1; m_scanned = true;
            try {
                std::string dir = app->getFileSystem()->GetAssetsPath() + "Skybox/";
                for (const auto& entry : fs::directory_iterator(dir)) {
                    if (!entry.is_regular_file()) continue;
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext != ".dds" && ext != ".hdr") continue;
                    m_skyboxFiles.push_back(entry.path().filename().string());
                    if (dir + m_skyboxFiles.back() == sky.cubemapPath)
                        m_selectedSkybox = (int)m_skyboxFiles.size() - 1;
                }
            } catch (...) {}
        }
        ImGui::SameLine(); textMuted("%d file(s)", (int)m_skyboxFiles.size());

        ImGui::BeginChild("##SkyList", ImVec2(0, 90), true);
        for (int i = 0; i < (int)m_skyboxFiles.size(); ++i)
            if (ImGui::Selectable(m_skyboxFiles[i].c_str(), m_selectedSkybox == i))
                m_selectedSkybox = i;
        ImGui::EndChild();

        if (m_selectedSkybox >= 0 && m_selectedSkybox < (int)m_skyboxFiles.size()) {
            std::string dir      = app->getFileSystem()->GetAssetsPath() + "Skybox/";
            std::string fullPath = dir + m_skyboxFiles[m_selectedSkybox];
            if (ImGui::Button("Load Skybox")) {
                sky.cubemapPath = fullPath;
                if (EnvironmentSystem* env = m_editor->getEnvSystem()) {
                    std::string ext = fs::path(fullPath).extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".hdr") env->loadHDR(fullPath);
                    else               env->load(fullPath);
                    m_editor->log(("Skybox: " + m_skyboxFiles[m_selectedSkybox]).c_str(),
                                  EditorColors::Success);
                }
            }
        } else { textMuted("No skybox selected"); }
        ImGui::Spacing();
    }

    // ================================================================
    // 2. LIGHTING — ambient (color + intensity on one row)
    // ================================================================
    if (accentHeader("Lighting", EditorColors::CBV)) {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 3));
        if (ImGui::BeginTable("##amb", 2, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("##lbl", ImGuiTableColumnFlags_WidthFixed, 70.f);
            ImGui::TableSetupColumn("##val", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); textMuted("Ambient");
            ImGui::TableSetColumnIndex(1);
            float w = ImGui::GetContentRegionAvail().x;
            ImGui::SetNextItemWidth(w * 0.52f);
            ImGui::ColorEdit3("##ambcol", &s.ambient.color.x, ImGuiColorEditFlags_NoLabel);
            ImGui::SameLine(0, 6.f);
            ImGui::SetNextItemWidth(-1.f);
            ImGui::SliderFloat("##ambint", &s.ambient.intensity, 0.f, 2.f, "%.2f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ambient intensity  (0 – 2)");
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        ImGui::Spacing();
    }

    // ================================================================
    // 3. PHYSICS — gravity display (constexpr, not runtime-settable)
    // ================================================================
    if (accentHeader("Physics", EditorColors::Hot)) {
        textMuted("Gravity Y");
        ImGui::SameLine(90.f);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Tx0);
        ImGui::Text("%.2f m/s²", ComponentRigidbody::kGravityAccel);
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 6.f);
        textMuted("(const)");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Set by ComponentRigidbody::kGravityAccel.\n"
                              "Adjust per-object with gravityScale on the Rigidbody.");
        ImGui::Spacing();
    }

    // ================================================================
    // 4. BROADPHASE — mode selection + per-mode parameter
    // ================================================================
    CollisionSystem* cs = m_editor->getCollisionSystem();
    if (accentHeader("Broadphase", EditorColors::DSV)) {
        if (!cs) { textMuted("No collision system."); return; }

        bool isBrute  = !cs->isUsingGrid() && !cs->isUsingOctree();
        bool isGrid   =  cs->isUsingGrid();
        bool isOctree =  cs->isUsingOctree();

        if (ImGui::RadioButton("Brute Force##ssp",  isBrute))  cs->useBruteForceBroadPhase();
        ImGui::SameLine(); textMuted("O(n²)");
        if (ImGui::RadioButton("Uniform Grid##ssp", isGrid))   cs->useGridBroadPhase();
        ImGui::SameLine(); textMuted("O(n log n)");
        if (ImGui::RadioButton("Octree##ssp",       isOctree)) cs->useOctreeBroadPhase();
        ImGui::SameLine(); textMuted("adaptive");

        ImGui::Spacing();
        if (cs->isUsingGrid()) {
            float cellSize = cs->getGridCellSize();
            ImGui::SetNextItemWidth(140.f);
            if (ImGui::DragFloat("Cell Size##sspcell", &cellSize, 0.1f, 0.5f, 64.f, "%.1f"))
                cs->setGridCellSize(cellSize);
        } else if (cs->isUsingOctree()) {
            int cap = cs->getOctreeNodeCapacity();
            ImGui::SetNextItemWidth(100.f);
            if (ImGui::DragInt("Leaf Cap##sspcap", &cap, 1, 1, 64))
                cs->setOctreeNodeCapacity(cap);
        }
        ImGui::Spacing();
    }
}
