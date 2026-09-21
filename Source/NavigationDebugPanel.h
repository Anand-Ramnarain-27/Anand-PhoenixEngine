#pragma once
#include "EditorPanel.h"

class NavigationDebugPanel : public EditorPanel {
public:
    explicit NavigationDebugPanel(ModuleEditor* editor)
        : EditorPanel(editor){ open = false; }
    const char* getName() const override { return "Navigation Debug"; }

protected:
    void drawContent() override;
};
