#pragma once
#include "EditorColors.h"
#include <imgui.h>
#include <algorithm>

class ModuleEditor;

class EditorPanel
{
public:
    explicit EditorPanel(ModuleEditor* editor) : m_editor(editor) {}
    virtual ~EditorPanel() = default;

    virtual void draw()
    {
        if (noPadding()) ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        if (ImGui::Begin(getName(), &open, windowFlags())) drawContent();
        ImGui::End();
        if (noPadding()) ImGui::PopStyleVar();
    }

    virtual const char* getName() const = 0;
    bool open = true;

protected:
    virtual void drawContent() {}
    virtual ImGuiWindowFlags windowFlags() const { return 0; }
    virtual bool noPadding() const { return false; }

    static void textColored(const ImVec4& col, const char* fmt, ...) { va_list a; va_start(a, fmt); ImGui::PushStyleColor(ImGuiCol_Text, col); ImGui::TextV(fmt, a); ImGui::PopStyleColor(); va_end(a); }
    static void textMuted(const char* fmt, ...) { va_list a; va_start(a, fmt); ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Muted);   ImGui::TextV(fmt, a); ImGui::PopStyleColor(); va_end(a); }
    static void textSuccess(const char* fmt, ...) { va_list a; va_start(a, fmt); ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Success); ImGui::TextV(fmt, a); ImGui::PopStyleColor(); va_end(a); }
    static void textDanger(const char* fmt, ...) { va_list a; va_start(a, fmt); ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Danger);  ImGui::TextV(fmt, a); ImGui::PopStyleColor(); va_end(a); }
    static void textWarning(const char* fmt, ...) { va_list a; va_start(a, fmt); ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Warning); ImGui::TextV(fmt, a); ImGui::PopStyleColor(); va_end(a); }
    static void textActive(const char* fmt, ...) { va_list a; va_start(a, fmt); ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Active);  ImGui::TextV(fmt, a); ImGui::PopStyleColor(); va_end(a); }

    static void logResult(ModuleEditor* ed, bool ok, const char* good, const char* bad);
    static std::string toLower(std::string s) { std::transform(s.begin(), s.end(), s.begin(), ::tolower); return s; }

    ModuleEditor* m_editor = nullptr;
};