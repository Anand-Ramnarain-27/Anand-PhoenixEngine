#include "Globals.h"
#include "SceneSettingsPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleCamera.h"
#include "SceneManager.h"
#include "EditorSceneSettings.h"
#include "EnvironmentSystem.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

void SceneSettingsPanel::drawContent()
{
    if (!m_editor->getSceneManager())
    {
        textMuted("No scene manager.");
        return;
    }
    drawDisplaySection();
    drawLightingSection();
    drawCameraSection();
    drawSkyboxSection();
}

void SceneSettingsPanel::drawDisplaySection()
{
    if (!ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) return;
    EditorSceneSettings& s = m_editor->getSceneManager()->getSettings();
    ImGui::Checkbox("Show Grid", &s.showGrid);
    ImGui::Checkbox("Show Axis", &s.showAxis);
    ImGui::Separator();
    ImGui::Text("Texture Sampler");
    int samplerType = m_editor->getSamplerType();
    if (ImGui::Combo("##sampler", &samplerType, "Linear/Wrap\0Point/Wrap\0Linear/Clamp\0Point/Clamp\0"))
        m_editor->setSamplerType(samplerType);
}

void SceneSettingsPanel::drawLightingSection()
{
    if (!ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) return;
    EditorSceneSettings& s = m_editor->getSceneManager()->getSettings();
    ImGui::Checkbox("Debug Draw Lights", &s.debugDrawLights);
    if (s.debugDrawLights) ImGui::SliderFloat("Light Size", &s.debugLightSize, 0.1f, 5.0f);
    ImGui::Separator();
    if (ImGui::TreeNodeEx("Ambient", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::ColorEdit3("Color", &s.ambient.color.x);
        ImGui::SliderFloat("Intensity", &s.ambient.intensity, 0, 2);
        ImGui::TreePop();
    }
    textMuted("Add light components via Inspector.");
}

void SceneSettingsPanel::drawCameraSection()
{
    if (!ImGui::CollapsingHeader("Camera & Culling", ImGuiTreeNodeFlags_DefaultOpen)) return;
    app->getCamera()->onEditorDebugPanel();
}

void SceneSettingsPanel::drawSkyboxSection()
{
    if (!ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen)) return;

    SceneManager* sm = m_editor->getSceneManager();
    EditorSceneSettings& s = sm->getSettings();
    auto& sky = s.skybox;

    ImGui::Checkbox("Enable Skybox", &sky.enabled);
    ImGui::Separator();

    if (!m_scanned || ImGui::Button("Refresh"))
    {
        m_skyboxFiles.clear();
        m_selectedSkybox = -1;
        m_scanned = true;
        try
        {
            for (const auto& entry : fs::directory_iterator("Assets/Skybox/"))
            {
                if (!entry.is_regular_file()) continue;
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext != ".dds") continue;
                m_skyboxFiles.push_back(entry.path().filename().string());
                if ("Assets/Skybox/" + m_skyboxFiles.back() == sky.cubemapPath)
                    m_selectedSkybox = (int)m_skyboxFiles.size() - 1;
            }
        }
        catch (...) { m_editor->log("[Editor] Could not scan Assets/Skybox/"); }
    }

    ImGui::SameLine();
    textMuted("%d file(s)", (int)m_skyboxFiles.size());

    ImGui::BeginChild("##SkyboxList", ImVec2(0, 120), true);
    for (int i = 0; i < (int)m_skyboxFiles.size(); ++i)
    {
        if (ImGui::Selectable(m_skyboxFiles[i].c_str(), m_selectedSkybox == i))
            m_selectedSkybox = i;
        if (m_selectedSkybox == i) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndChild();

    if (m_selectedSkybox >= 0 && m_selectedSkybox < (int)m_skyboxFiles.size())
    {
        std::string fullPath = "Assets/Skybox/" + m_skyboxFiles[m_selectedSkybox];
        textMuted("Path: %s", fullPath.c_str());
        if (ImGui::Button("Load Selected Skybox"))
        {
            sky.cubemapPath = fullPath;
            if (EnvironmentSystem* env = m_editor->getEnvSystem())
            {
                env->load(sky.cubemapPath);
                m_editor->log(("Skybox loaded: " + m_skyboxFiles[m_selectedSkybox]).c_str(),
                    EditorColors::Success);
            }
        }
    }
    else
    {
        textMuted("No skybox selected");
    }
}