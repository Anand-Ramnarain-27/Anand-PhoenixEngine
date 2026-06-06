#include "Globals.h"
#include "SceneSettingsPanel.h"
#include "BloomPass.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "SceneManager.h"
#include "EditorSceneSettings.h"
#include "EnvironmentSystem.h"
#include "CollisionSystem.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

void SceneSettingsPanel::drawContent() {
    if (!m_editor->getSceneManager()) { textMuted("No scene manager."); return; }
    drawEnvironmentSection();
    drawLightingSection();
    drawPostProcessSection();
    drawPhysicsSection();
    drawBroadphaseSection();
}

// ---- Post-Processing (Bloom) -----------------------------------------------
void SceneSettingsPanel::drawPostProcessSection() {
    if (!ImGui::CollapsingHeader("Post-Processing")) return;
    auto* bloom = m_editor->getBloomPass();
    if (!bloom) { ImGui::TextDisabled("Bloom pass not available."); return; }
    auto& s = bloom->getSettings();
    ImGui::Checkbox("Bloom Enabled",   &s.enabled);
    if (s.enabled) {
        ImGui::SliderFloat("Threshold", &s.threshold, 0.0f, 1.0f);
        ImGui::SliderFloat("Strength",  &s.strength,  0.0f, 5.0f);
    }
}

// ---- 1. Environment --------------------------------------------------------
void SceneSettingsPanel::drawEnvironmentSection() {
    if (!ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) return;
    SceneManager*         sm  = m_editor->getSceneManager();
    EditorSceneSettings&  s   = sm->getSettings();
    auto&                 sky = s.skybox;

    // Lazy-scan Assets/Skybox/ for .dds / .hdr files.
    // Choosing a file auto-enables the skybox; choosing "(none)" disables it.
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
        }
        catch (...) { m_editor->log("[Editor] Could not scan Assets/Skybox/"); }
    }
    ImGui::SameLine(); textMuted("%d file(s)", (int)m_skyboxFiles.size());

    // Compact combo — first entry is always "(none)" which disables the skybox.
    const char* preview = (m_selectedSkybox >= 0 && m_selectedSkybox < (int)m_skyboxFiles.size())
        ? m_skyboxFiles[m_selectedSkybox].c_str() : "(none)";
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::BeginCombo("##skybox_pick", preview)) {
        // None entry
        bool noneSelected = (m_selectedSkybox < 0);
        if (ImGui::Selectable("(none)", noneSelected)) {
            m_selectedSkybox = -1;
            sky.enabled      = false;
            sky.cubemapPath.clear();
        }
        if (noneSelected) ImGui::SetItemDefaultFocus();

        for (int i = 0; i < (int)m_skyboxFiles.size(); ++i) {
            bool sel = (m_selectedSkybox == i);
            if (ImGui::Selectable(m_skyboxFiles[i].c_str(), sel)) {
                m_selectedSkybox = i;
                std::string dir      = app->getFileSystem()->GetAssetsPath() + "Skybox/";
                std::string fullPath = dir + m_skyboxFiles[i];
                sky.cubemapPath = fullPath;
                sky.enabled     = true;   // auto-enable when a file is chosen
                if (EnvironmentSystem* env = m_editor->getEnvSystem()) {
                    std::string ext = fs::path(fullPath).extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".hdr") env->loadHDR(fullPath);
                    else               env->load(fullPath);
                    m_editor->log(("Skybox loaded: " + m_skyboxFiles[i]).c_str(), EditorColors::Success);
                }
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

// ---- 2. Lighting -----------------------------------------------------------
void SceneSettingsPanel::drawLightingSection() {
    if (!ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) return;
    EditorSceneSettings& s = m_editor->getSceneManager()->getSettings();

    // Ambient: color swatch + intensity on one row.
    ImGui::Text("Ambient");
    ImGui::SameLine(80.f);
    ImGui::SetNextItemWidth(28.f);
    ImGui::ColorEdit3("##amb_col", &s.ambient.color.x,
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.f);
    ImGui::SliderFloat("##amb_int", &s.ambient.intensity, 0.f, 2.f, "%.2f");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ambient intensity");
}

// ---- 3. Physics ------------------------------------------------------------
void SceneSettingsPanel::drawPhysicsSection() {
    if (!ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) return;
    EditorSceneSettings& s = m_editor->getSceneManager()->getSettings();

    ImGui::Text("Gravity Y");
    ImGui::SameLine(80.f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::DragFloat("##gravity_y", &s.gravityY, 0.1f, -50.f, 0.f, "%.2f m/s²");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("World-space Y gravity (m/s²).\nApplied by ComponentRigidbody each frame.");
}

// ---- 4. Broadphase ---------------------------------------------------------
void SceneSettingsPanel::drawBroadphaseSection() {
    if (!ImGui::CollapsingHeader("Broadphase", ImGuiTreeNodeFlags_DefaultOpen)) return;
    CollisionSystem* cs = m_editor->getCollisionSystem();
    if (!cs) { textMuted("No collision system."); return; }

    // Three-button selector.
    const bool isGrid    = cs->isUsingGrid();
    const bool isOctree  = cs->isUsingOctree();
    const bool isBrute   = !isGrid && !isOctree;

    auto styleActive = [](bool on) {
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::Active);
    };
    auto styleEnd = [](bool on) {
        if (on) ImGui::PopStyleColor();
    };

    styleActive(isBrute);  if (ImGui::Button("Brute Force"))  { cs->useBruteForceBroadPhase(); } styleEnd(isBrute);
    ImGui::SameLine();
    styleActive(isGrid);   if (ImGui::Button("Uniform Grid")) { cs->useGridBroadPhase(); }       styleEnd(isGrid);
    ImGui::SameLine();
    styleActive(isOctree); if (ImGui::Button("Octree"))       { cs->useOctreeBroadPhase(); }     styleEnd(isOctree);

    // Complexity label.
    const char* complexity = isBrute ? "O(n\xC2\xB2)" : isGrid ? "O(n log n)" : "adaptive";
    ImGui::SameLine(0, 10);
    textMuted("%s", complexity);
}
