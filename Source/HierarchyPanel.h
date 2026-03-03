#pragma once
#include "EditorPanel.h"

class GameObject;

class HierarchyPanel : public EditorPanel
{
public:
    explicit HierarchyPanel(ModuleEditor* editor) : EditorPanel(editor) {}
    void draw() override;
    const char* getName() const override { return "Hierarchy"; }

private:
    void drawNode(GameObject* go);
    void itemContextMenu(GameObject* go);
    void blankContextMenu();
};