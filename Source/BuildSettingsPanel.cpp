#include "Globals.h"
#include "BuildSettingsPanel.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "FileDialog.h"
#include "AssetPickerWidget.h"
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

static std::string toRelativeAssetPath(const std::string& absolutePath){
    std::string assetsPath = app->getFileSystem()->GetAssetsPath();
    std::string baseDir = assetsPath.substr(0, assetsPath.size() - std::string("Assets/").size());
    if (absolutePath.size() > baseDir.size() && absolutePath.compare(0, baseDir.size(), baseDir) == 0)
        return absolutePath.substr(baseDir.size());
    return absolutePath;
}

BuildSettingsPanel::BuildSettingsPanel(ModuleEditor* editor) : EditorPanel(editor){
    m_settingsPath = app->getFileSystem()->GetLibraryPath() + "BuildSettings.json";
    m_settings.Load(m_settingsPath);
    m_outputDirDialog = std::make_unique<FileDialog>();
}

void BuildSettingsPanel::save(){
    m_settings.Save(m_settingsPath);
}

void BuildSettingsPanel::drawContent(){
    drawSceneList();
    ImGui::Spacing();
    drawOutputSection();
}

void BuildSettingsPanel::drawSceneList(){
    ImGui::SeparatorText("Scenes in Build");

    int removeIdx = -1, moveUp = -1, moveDown = -1;
    for (int i = 0; i < (int)m_settings.scenes.size(); ++i){
        BuildSceneEntry& entry = m_settings.scenes[i];
        ImGui::PushID(i);

        if (ImGui::Checkbox("##enabled", &entry.enabled)) save();

        ImGui::SameLine();
        std::string label = std::to_string(i) + ": " + fs::path(entry.path).filename().string();
        ImGui::Selectable(label.c_str(), false, 0, ImVec2(ImGui::GetContentRegionAvail().x - 100.f, 0));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.path.c_str());

        ImGui::SameLine();
        ImGui::BeginDisabled(i == 0);
        if (ImGui::SmallButton("^")) moveUp = i;
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(i == (int)m_settings.scenes.size() - 1);
        if (ImGui::SmallButton("v")) moveDown = i;
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::SmallButton("X")) removeIdx = i;

        ImGui::PopID();
    }

    if (removeIdx >= 0){ m_settings.scenes.erase(m_settings.scenes.begin() + removeIdx); save(); }
    if (moveUp > 0){ std::swap(m_settings.scenes[moveUp], m_settings.scenes[moveUp - 1]); save(); }
    if (moveDown >= 0 && moveDown + 1 < (int)m_settings.scenes.size()){ std::swap(m_settings.scenes[moveDown], m_settings.scenes[moveDown + 1]); save(); }

    if (m_settings.scenes.empty()) textMuted("No scenes added yet.");

    ImGui::Spacing();
    ImGui::TextUnformatted("Add scene:");
    ImGui::SameLine();
    if (AssetPicker::Draw("##addscene", m_pickedScenePath, AssetPicker::kScenes)){
        if (!m_pickedScenePath.empty()){
            bool exists = std::any_of(m_settings.scenes.begin(), m_settings.scenes.end(),
                [&](const BuildSceneEntry& e){ return e.path == m_pickedScenePath; });
            if (!exists){ m_settings.scenes.push_back({ m_pickedScenePath, true }); save(); }
            m_pickedScenePath.clear();
        }
    }

    if (m_editor && !m_editor->getCurrentScenePath().empty()){
        ImGui::SameLine();
        if (ImGui::Button("Add Current Scene")){
            std::string path = toRelativeAssetPath(m_editor->getCurrentScenePath());
            bool exists = std::any_of(m_settings.scenes.begin(), m_settings.scenes.end(),
                [&](const BuildSceneEntry& e){ return e.path == path; });
            if (!exists){ m_settings.scenes.push_back({ path, true }); save(); }
        }
    }
}

void BuildSettingsPanel::drawOutputSection(){
    ImGui::SeparatorText("Output");

    static const char* kConfigs[] = { "Debug", "Release" };
    int configIdx = (m_settings.configuration == "Debug") ? 0 : 1;
    ImGui::SetNextItemWidth(160.f);
    if (ImGui::Combo("Configuration", &configIdx, kConfigs, 2)){
        m_settings.configuration = kConfigs[configIdx];
        save();
    }

    ImGui::SetNextItemWidth(160.f);
    ImGui::BeginDisabled(true);
    char platformBuf[8] = "x64";
    ImGui::InputText("Platform", platformBuf, sizeof(platformBuf));
    ImGui::EndDisabled();

    char outBuf[512];
    strncpy(outBuf, m_settings.outputDir.c_str(), sizeof(outBuf) - 1);
    outBuf[sizeof(outBuf) - 1] = '\0';
    ImGui::SetNextItemWidth(-90.f);
    if (ImGui::InputText("##outputdir", outBuf, sizeof(outBuf))){
        m_settings.outputDir = outBuf;
        save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) m_outputDirDialog->open(FileDialog::Type::SelectFolder, "Select Output Folder", m_settings.outputDir.empty() ? "." : m_settings.outputDir);

    if (m_outputDirDialog->draw()){
        m_settings.outputDir = m_outputDirDialog->getSelectedPath();
        save();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    BuildPipeline::Status status = BuildPipeline::Get().GetStatus();
    if (status != m_lastSeenStatus){
        if (status == BuildPipeline::Status::Success)
            m_editor->log(BuildPipeline::Get().GetMessage().c_str(), EditorColors::Success);
        else if (status == BuildPipeline::Status::Failed)
            m_editor->log(("Build failed: " + BuildPipeline::Get().GetMessage()).c_str(), EditorColors::Danger);
        m_lastSeenStatus = status;
    }

    bool isRunning = BuildPipeline::Get().IsRunning();
    int enabledCount = m_settings.getEnabledSceneCount();
    ImGui::BeginDisabled(enabledCount == 0 || m_settings.outputDir.empty() || isRunning);
    if (ImGui::Button("Build", ImVec2(120, 32))){
        save();
        m_editor->log("Build started: compiling Player and packaging assets...", EditorColors::Info);
        BuildPipeline::Get().StartBuild(m_settings);
    }
    ImGui::EndDisabled();

    if (isRunning){
        ImGui::Spacing();
        ImGui::ProgressBar(BuildPipeline::Get().GetProgress(), ImVec2(-1.f, 0.f));
        ImGui::TextWrapped("%s", BuildPipeline::Get().GetMessage().c_str());
    }
    else if (enabledCount == 0){ ImGui::SameLine(); textMuted("Add at least one enabled scene."); }
    else if (m_settings.outputDir.empty()){ ImGui::SameLine(); textMuted("Choose an output folder."); }
}
