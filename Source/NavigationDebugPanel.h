#pragma once
// Editor panel for the AI navigation system: graph, agents and debug draw toggles.

#include "EditorPanel.h"

class NavigationDebugPanel : public EditorPanel {
public:
    explicit NavigationDebugPanel(ModuleEditor* editor)
        : EditorPanel(editor){ open = false; }
    const char* getName() const override { return "Navigation Debug"; }

protected:
    void drawContent() override;
};
