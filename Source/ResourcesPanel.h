#pragma once
#include "EditorPanel.h"
#include "ResourceCommon.h"

class ResourcesPanel : public EditorPanel {
public:
    explicit ResourcesPanel(ModuleEditor* editor) : EditorPanel(editor){}
    const char* getName() const override { return "Resources"; }

protected:
    void drawContent() override;

private:
    static ImVec4 typeColor(ResourceBase::Type t);
    static const char* typeName(ResourceBase::Type t);
};
