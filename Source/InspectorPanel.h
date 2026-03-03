#pragma once
#include "EditorPanel.h"

class ComponentCamera;
class ComponentMesh;

class InspectorPanel : public EditorPanel
{
public:
    explicit InspectorPanel(ModuleEditor* editor) : EditorPanel(editor) {}
    void draw() override;
    const char* getName() const override { return "Inspector"; }

private:
    void drawTransform();
    void drawComponentCamera(ComponentCamera* cam);
    void drawComponentMesh(ComponentMesh* mesh);
    void drawAddComponentMenu();
};