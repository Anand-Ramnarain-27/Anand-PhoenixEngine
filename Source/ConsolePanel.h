#pragma once
#include "EditorPanel.h"
#include <vector>
#include <string>
#include <imgui.h>

struct ConsoleEntry { std::string text; ImVec4 color; };

class ConsolePanel : public EditorPanel
{
public:
    explicit ConsolePanel(ModuleEditor* editor) : EditorPanel(editor) {}
    void draw() override;
    const char* getName() const override { return "Console"; }
    void add(const char* text, const ImVec4& color = ImVec4(1, 1, 1, 1));
    void clear() { m_entries.clear(); }

private:
    std::vector<ConsoleEntry> m_entries;
    bool m_autoScroll = true;
};