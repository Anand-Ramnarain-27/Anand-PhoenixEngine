#pragma once
#include <imgui.h>

class ModuleEditor;

class EditorPanel
{
public:
    explicit EditorPanel(ModuleEditor* editor) : m_editor(editor) {}
    virtual ~EditorPanel() = default;

    virtual void draw() = 0;
    virtual const char* getName() const = 0;

    bool open = true;

protected:
    ModuleEditor* m_editor = nullptr;
};