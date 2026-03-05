#include "Globals.h"
#include "EditorPanel.h"
#include "ModuleEditor.h"
#include <algorithm>

void EditorPanel::logResult(ModuleEditor* ed, bool ok, const char* good, const char* bad) {
    ed->log(ok ? good : bad, ok ? EditorColors::Success : EditorColors::Danger);
}

std::string EditorPanel::toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}