#include "Globals.h"
#include "BuildSettingsPanel.h"
#include "EditorColors.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "FileDialog.h"
#include "AssetPickerWidget.h"
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// A build-list path ("Library/Scenes/AF_X.json", relative to the project folder, or absolute) that exists?
static bool sceneFileExists(const std::string& path){
    if (path.empty()) return false;
    std::error_code ec;
    if (fs::path(path).is_absolute()) return fs::exists(path, ec);
    std::string assetsPath = app->getFileSystem()->GetAssetsPath();
    std::string baseDir = assetsPath.substr(0, assetsPath.size() - std::string("Assets/").size());
    return fs::exists(baseDir + path, ec);
}

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
        const bool missing = !sceneFileExists(entry.path);
        if (missing){ label += "  (missing)"; ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Danger); }
        ImGui::Selectable(label.c_str(), false, 0, ImVec2(ImGui::GetContentRegionAvail().x - 100.f, 0));
        if (missing) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(missing ? "%s\nNot found: remove it and add the scene again (scenes live in Library/Scenes)." : "%s", entry.path.c_str());

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
    // Scenes are saved in Library/Scenes: list those (not every .json under Assets).
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
    if (ImGui::BeginCombo("##addscene", "Pick a scene...", ImGuiComboFlags_HeightLarge)){
        std::vector<std::string> names;
        std::error_code ec;
        for (const auto& f : fs::directory_iterator(app->getFileSystem()->GetLibraryPath() + "Scenes", ec)){
            if (!f.is_regular_file() || f.path().extension() != ".json") continue;
            const std::string stem = f.path().stem().string();
            if (stem != "temp_scene") names.push_back(stem);
        }
        std::sort(names.begin(), names.end());
        for (const std::string& n : names){
            const std::string path = "Library/Scenes/" + n + ".json";
            const bool inList = std::any_of(m_settings.scenes.begin(), m_settings.scenes.end(),
                [&](const BuildSceneEntry& e){ return e.path == path; });
            if (ImGui::Selectable(n.c_str(), false, inList ? ImGuiSelectableFlags_Disabled : 0)){
                m_settings.scenes.push_back({ path, true });
                save();
            }
        }
        ImGui::EndCombo();
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

    char nameBuf[128];
    strncpy(nameBuf, m_settings.productName.c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::InputText("Product Name", nameBuf, sizeof(nameBuf))){
        m_settings.productName = nameBuf;
        save();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Name of the built .exe (e.g. \"MyGame\" -> MyGame.exe).");

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

    if (ImGui::Checkbox("Strip source assets", &m_settings.stripSourceAssets)) save();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Skip shipping raw source models/textures (.fbx/.gltf/.png/.jpg/...) that are\n"
                           "already baked into Library/. Scenes, scripts, and skybox HDRs still ship.\n"
                           "Shrinks the build significantly; turn off if something goes missing.");

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
