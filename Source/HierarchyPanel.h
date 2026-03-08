#pragma once
#include "EditorPanel.h"

class GameObject;

class HierarchyPanel : public EditorPanel {
public:
    explicit HierarchyPanel(ModuleEditor* editor) : EditorPanel(editor) {}
    const char* getName() const override { return "Hierarchy"; }

protected:
    void drawContent() override;

private:
    void drawNode(GameObject* go, bool prefabMode = false, bool isRoot = false);
    void itemContextMenu(GameObject* go);
    void blankContextMenu();
};