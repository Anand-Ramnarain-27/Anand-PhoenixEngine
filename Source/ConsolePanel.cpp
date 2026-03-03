#include "Globals.h"
#include "ConsolePanel.h"

void ConsolePanel::add(const char* text, const ImVec4& color)
{
    m_entries.push_back({ text, color });
}

void ConsolePanel::draw()
{
    ImGui::Begin("Console", &open);
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
    ImGui::End();
}