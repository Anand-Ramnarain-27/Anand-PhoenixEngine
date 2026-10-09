#include "Globals.h"
#include "AshfallDataPanel.h"
#include "AshfallData.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include <imgui.h>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

void AshfallDataPanel::draw(){
    // "Open in Ashfall Data" from the Inspector opens and fronts the window on that file.
    const std::string focus = AshfallData::TakeFocusRequest();
    if (!focus.empty()){
        m_selected = focus;
        open = true;
        m_focusNext = true;
    }
    if (!open) return;
    if (m_focusNext){ ImGui::SetNextWindowFocus(); m_focusNext = false; }
    ImGui::SetNextWindowSize(ImVec2(760.f, 620.f), ImGuiCond_FirstUseEver);
    EditorPanel::draw();
}

void AshfallDataPanel::rescan(){
    m_groups = {
        { "Enemies", "Assets/Enemies", ".json" },
        { "Characters", "Assets/CharacterConfigs", ".json" },
        { "Party", "Assets/Party", ".json" },
        { "Encounters", "Assets/Encounters", ".json" },
        { "Traps", "Assets/Traps", ".json" },
        { "VFX recipes (read-only)", "Assets/VFX/recipes", ".json", true },
    };
    fs::path assets = fs::path(app->getFileSystem()->GetAssetsPath());
    if (!assets.has_filename()) assets = assets.parent_path();
    const fs::path root = assets.parent_path();
    std::error_code ec;
    for (Group& g : m_groups){
        g.files.clear();
        for (const auto& e : fs::directory_iterator(root / g.folder, ec)){
            if (!e.is_regular_file()) continue;
            const std::string name = e.path().filename().string();
            if (e.path().extension() != g.suffix || name == "_schema.json" || name.find(".schema.") != std::string::npos) continue;
            g.files.push_back(g.folder + "/" + name);
        }
        std::sort(g.files.begin(), g.files.end());
    }
    m_lastScan = ImGui::GetTime();
}

void AshfallDataPanel::drawContent(){
    if (ImGui::GetTime() - m_lastScan > 3.0) rescan();

    ImGui::BeginChild("##list", ImVec2(210.f, 0.f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    drawList();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##editor", ImVec2(0.f, 0.f));
    drawEditor();
    ImGui::EndChild();
}

void AshfallDataPanel::drawList(){
    if (ImGui::SmallButton("Refresh")) rescan();
    if (AshfallData::IsPlaying()){
        ImGui::SameLine();
        ImGui::TextColored(EditorColors::Warning, "live");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Playing: edits go to the running game at once and are not saved until you Save.");
    }
    for (const Group& g : m_groups){
        if (g.files.empty()) continue;
        if (!ImGui::CollapsingHeader(g.title.c_str(), g.readOnly ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) continue;
        for (const std::string& f : g.files){
            const AshfallData::Doc* d = nullptr;
            for (const auto& o : AshfallData::OpenDocs()) if (o->assetPath == f) d = o.get();
            std::string label = fs::path(f).filename().string();
            if (d && d->dirty) label += " *";
            if (d && d->editedInPlay) label += " (play)";
            if (ImGui::Selectable((label + "##" + f).c_str(), m_selected == f)) m_selected = f;
        }
    }
}

void AshfallDataPanel::drawEditor(){
    if (m_selected.empty()){
        textMuted("Pick a file on the left.");
        textMuted("Playing: edits apply to the running game live; Save writes the file.");
        return;
    }
    AshfallData::Doc* doc = AshfallData::Open(m_selected);
    if (!doc){ textDanger("can't open %s", m_selected.c_str()); return; }

    ImGui::TextUnformatted(doc->assetPath.c_str());
    if (!doc->schemaSource.empty()){
        ImGui::SameLine();
        textMuted("  schema: %s", doc->schemaSource.c_str());
    }
    AshfallData::DrawToolbar(*doc, false);
    if (!doc->readOnly){
        ImGui::SameLine();
        if (ImGui::SmallButton("Duplicate...")){
            const std::string stem = fs::path(doc->assetPath).filename().string();
            snprintf(m_dupName, sizeof(m_dupName), "%s_copy", stem.substr(0, stem.find('.')).c_str());
            ImGui::OpenPopup("Duplicate");
        }
        if (ImGui::BeginPopup("Duplicate")){
            ImGui::TextUnformatted("New name (same folder):");
            ImGui::SetNextItemWidth(240.f);
            ImGui::InputText("##dup", m_dupName, sizeof(m_dupName));
            if (ImGui::Button("Create")){
                if (AshfallData::Doc* copy = AshfallData::Duplicate(*doc, m_dupName, m_message)){
                    m_selected = copy->assetPath;
                    rescan();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    if (!m_message.empty()) textMuted("%s", m_message.c_str());
    ImGui::Separator();

    ImGui::BeginChild("##tree");
    AshfallData::DrawTree(*doc, nullptr);
    ImGui::EndChild();
}
