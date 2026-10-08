#pragma once
// The Console panel: editor and engine log messages, with per-category log levels.

#include "EditorPanel.h"
#include <vector>
#include <string>

struct ConsoleEntry { std::string text; ImVec4 color; };

/// Shows the lines ModuleEditor::log() adds (PHX_LOG output reaches it through the console sink).
class ConsolePanel : public EditorPanel {
public:
    explicit ConsolePanel(ModuleEditor* editor) : EditorPanel(editor){}
    const char* getName() const override { return "Console"; }

    void add(const char* text, const ImVec4& color = EditorColors::White){ m_entries.push_back({ text, color }); }
    void clear(){ m_entries.clear(); }

protected:
    void drawContent() override{
        if (ImGui::Button("Clear")) clear();
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll);
        ImGui::SameLine();
        if (ImGui::Button("Log levels")) ImGui::OpenPopup("##logLevels");
        drawLogLevelsPopup();
        ImGui::Separator();
        ImGui::BeginChild("##scroll");
        for (const auto& e : m_entries){
            ImGui::PushStyleColor(ImGuiCol_Text, e.color);
            ImGui::TextUnformatted(e.text.c_str());
            ImGui::PopStyleColor();
        }
        if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }

private:
    // One level combo per category: engine categories first, then the script categories seen so far.
    // Changes apply to messages logged from then on.
    static void drawLogLevelsPopup(){
        if (!ImGui::BeginPopup("##logLevels")) return;
        static const char* const kLevels[] = { "Error", "Warning", "Info", "Verbose" };
        ImGui::TextDisabled("Engine");
        for (int i = 0; i < static_cast<int>(LogCategory::Count); ++i){
            const auto category = static_cast<LogCategory>(i);
            int level = static_cast<int>(PhoenixLog::threshold(category));
            ImGui::SetNextItemWidth(110.f);
            if (ImGui::Combo(PhoenixLog::categoryName(category), &level, kLevels, IM_ARRAYSIZE(kLevels)))
                PhoenixLog::setThreshold(category, static_cast<LogLevel>(level));
        }
        const auto scripts = PhoenixLog::scriptCategories();
        if (!scripts.empty()){
            ImGui::Separator();
            ImGui::TextDisabled("Game scripts");
            for (const auto& c : scripts){
                int level = static_cast<int>(c.threshold);
                ImGui::SetNextItemWidth(110.f);
                if (ImGui::Combo(("##script" + c.name).c_str(), &level, kLevels, IM_ARRAYSIZE(kLevels)))
                    PhoenixLog::setScriptThreshold(c.name, static_cast<LogLevel>(level));
                ImGui::SameLine();
                ImGui::TextUnformatted(c.name.c_str());
            }
        }
        ImGui::EndPopup();
    }

    std::vector<ConsoleEntry> m_entries;
    bool m_autoScroll = true;
};
