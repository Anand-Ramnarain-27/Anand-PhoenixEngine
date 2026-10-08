#pragma once
// Window > Ashfall Data: browse and edit the game's JSON data files.

#include "EditorPanel.h"
#include <string>
#include <vector>

/// Window > Ashfall Data: the game's JSON data (enemies, characters, party, encounters; VFX recipes read-only) with
/// typed, schema-aware widgets, Save / Revert / Duplicate, and live tuning while playing (AshfallData.h,
/// docs/ASHFALL_DATA.md).
class AshfallDataPanel : public EditorPanel {
public:
    explicit AshfallDataPanel(ModuleEditor* editor) : EditorPanel(editor){ open = false; }
    const char* getName() const override { return "Ashfall Data"; }
    void draw() override;

protected:
    void drawContent() override;

private:
    struct Group { std::string title, folder, suffix; bool readOnly = false; std::vector<std::string> files; };
    void rescan();
    void drawList();
    void drawEditor();

    std::vector<Group> m_groups;
    std::string m_selected;   // asset path
    double m_lastScan = -100.0;
    char m_dupName[96] = "";
    std::string m_message;
    bool m_focusNext = false;
};
