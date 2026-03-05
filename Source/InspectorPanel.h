#pragma once
#include "EditorPanel.h"

class ComponentCamera;
class ComponentMesh;

class InspectorPanel : public EditorPanel
{
public:
    explicit InspectorPanel(ModuleEditor* editor) : EditorPanel(editor) {}
    const char* getName() const override { return "Inspector"; }

protected:
    void drawContent() override;

private:
    void drawTransform();
    void drawComponentCamera(ComponentCamera* cam);
    void drawComponentMesh(ComponentMesh* mesh);
    void drawAddComponentMenu();

};