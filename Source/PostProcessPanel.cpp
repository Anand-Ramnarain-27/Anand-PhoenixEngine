#include "Globals.h"
#include "PostProcessPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "SceneManager.h"
#include "EditorSceneSettings.h"
#include "PostProcessChain.h"
#include "ColorLUT.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

void PostProcessPanel::drawContent(){
    if (!m_editor->getSceneManager()){ textMuted("No scene manager."); return; }
    drawTonemapSection();
    drawBloomSection();
    drawLutSection();
    drawPluginEffectsSection();
}

void PostProcessPanel::drawTonemapSection(){
    if (!ImGui::CollapsingHeader("Exposure & Tonemap", ImGuiTreeNodeFlags_DefaultOpen)) return;
    EditorSceneSettings& s = m_editor->getSceneManager()->getSettings();

    ImGui::Text("Exposure");
    ImGui::SameLine(100.f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::SliderFloat("##exposure", &s.postProcess.exposure, -6.f, 6.f, "%.2f EV");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Exposure value in stops. Each +1 doubles scene brightness before tonemapping.");
}

void PostProcessPanel::drawBloomSection(){
    if (!ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen)) return;
    EditorSceneSettings& s = m_editor->getSceneManager()->getSettings();
    auto& bloom = s.postProcess;

    ImGui::Checkbox("Enabled##bloom", &bloom.bloomEnabled);

    ImGui::BeginDisabled(!bloom.bloomEnabled);
    ImGui::Text("Threshold");
    ImGui::SameLine(100.f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::SliderFloat("##bloom_thresh", &bloom.bloomThreshold, 0.f, 5.f, "%.2f");

    ImGui::Text("Intensity");
    ImGui::SameLine(100.f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::SliderFloat("##bloom_int", &bloom.bloomIntensity, 0.f, 3.f, "%.2f");
    ImGui::EndDisabled();
}

void PostProcessPanel::drawLutSection(){
    if (!ImGui::CollapsingHeader("Colour Grading (LUT)", ImGuiTreeNodeFlags_DefaultOpen)) return;
    EditorSceneSettings& s = m_editor->getSceneManager()->getSettings();
    auto& pp = s.postProcess;

    if (!m_scannedLuts || ImGui::Button("Refresh##lut")){
        m_lutFiles.clear(); m_selectedLut = -1; m_scannedLuts = true;
        try {
            std::string dir = app->getFileSystem()->GetAssetsPath() + "PostProcess/LUTs/";
            app->getFileSystem()->CreateDir(dir.c_str());
            for (const auto& entry : fs::directory_iterator(dir)){
                if (!entry.is_regular_file() || entry.path().extension() != ".cube") continue;
                m_lutFiles.push_back(entry.path().filename().string());
                if (dir + m_lutFiles.back() == pp.lutPath)
                    m_selectedLut = (int)m_lutFiles.size() - 1;
            }
        }
        catch (...){ m_editor->log("[Editor] Could not scan Assets/PostProcess/LUTs/"); }
    }
    ImGui::SameLine(); textMuted("%d file(s)", (int)m_lutFiles.size());

    const char* preview = (m_selectedLut >= 0 && m_selectedLut < (int)m_lutFiles.size())
        ? m_lutFiles[m_selectedLut].c_str() : "(none)";
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::BeginCombo("##lut_pick", preview)){
        bool noneSelected = (m_selectedLut < 0);
        if (ImGui::Selectable("(none)", noneSelected)){
            m_selectedLut = -1;
            pp.lutEnabled = false;
            pp.lutPath.clear();
        }
        if (noneSelected) ImGui::SetItemDefaultFocus();

        for (int i = 0; i < (int)m_lutFiles.size(); ++i){
            bool sel = (m_selectedLut == i);
            if (ImGui::Selectable(m_lutFiles[i].c_str(), sel)){
                m_selectedLut = i;
                std::string dir = app->getFileSystem()->GetAssetsPath() + "PostProcess/LUTs/";
                std::string fullPath = dir + m_lutFiles[i];
                if (ColorLUT* lut = m_editor->getColorLUT()){
                    if (lut->loadCube(app->getD3D12()->getDevice(), fullPath)){
                        pp.lutPath = fullPath;
                        pp.lutEnabled = true;
                        m_editor->log(("LUT loaded: " + m_lutFiles[i]).c_str(), EditorColors::Success);
                    }
                    else m_editor->log(("Failed to load LUT: " + m_lutFiles[i]).c_str(), EditorColors::Danger);
                }
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::BeginDisabled(pp.lutPath.empty());
    ImGui::Checkbox("Enabled##lut", &pp.lutEnabled);
    ImGui::EndDisabled();
}

void PostProcessPanel::drawPluginEffectsSection(){
    if (!ImGui::CollapsingHeader("Plugin Effects", ImGuiTreeNodeFlags_DefaultOpen)) return;
    PostProcessChain* chain = m_editor->getPostProcessChain();
    if (!chain){ textMuted("No post-process chain."); return; }

    textMuted("Discovered from Assets/PostProcess/*.json - drop new .json + .cso pairs\nin that folder and hit Reload. No engine rebuild required.");

    if (ImGui::Button("Reload##postfx")) chain->reload(app->getD3D12()->getDevice());

    ImGui::Spacing();
    for (auto& effect : chain->getEffects()){
        ImGui::PushID(effect.def.name.c_str());
        ImGui::Checkbox("##enabled", &effect.def.enabled);
        ImGui::SameLine();
        ImGui::Text("%s", effect.def.name.c_str());
        ImGui::SameLine(220.f);
        textMuted("%s  order %d", effect.def.domain == PostProcessEffectDef::Domain::PreTonemap ? "Pre-tonemap" : "Post-gamma", effect.def.order);
        ImGui::PopID();
    }
    if (chain->getEffects().empty()) textMuted("(none found)");
}
