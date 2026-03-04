#pragma once
#include "EditorPanel.h"

class ResourcesPanel : public EditorPanel
{
public:
    explicit ResourcesPanel(ModuleEditor* editor) : EditorPanel(editor) {}
    void draw() override;
    const char* getName() const override { return "Resources"; }
};