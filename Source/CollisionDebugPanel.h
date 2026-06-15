#pragma once
#include "EditorPanel.h"

class CollisionDebugPanel : public EditorPanel {
public:
    explicit CollisionDebugPanel(ModuleEditor* editor)
        : EditorPanel(editor){ open = false; }
    const char* getName() const override { return "Collision Debug"; }

protected:
    void drawContent() override;
};
