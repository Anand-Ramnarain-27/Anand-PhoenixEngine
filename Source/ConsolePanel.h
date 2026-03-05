#pragma once
#include "EditorPanel.h"
#include <vector>
#include <string>

struct ConsoleEntry { std::string text; ImVec4 color; };

class ConsolePanel : public EditorPanel
{
public:
    explicit ConsolePanel(ModuleEditor* editor) : EditorPanel(editor) {}
    const char* getName() const override { return "Console"; }

    void add(const char* text, const ImVec4& color = EditorColors::White)
    {
        m_entries.push_back({ text, color });
    }
    void clear() { m_entries.clear(); }

protected:
    void drawContent() override
    {
        if (ImGui::Button("Clear")) clear();
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll);
        ImGui::Separator();

        ImGui::BeginChild("##scroll");
        for (const auto& e : m_entries)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, e.color);
            ImGui::TextUnformatted(e.text.c_str());
            ImGui::PopStyleColor();
        }
        if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }

private:
    std::vector<ConsoleEntry> m_entries;
    bool m_autoScroll = true;
};