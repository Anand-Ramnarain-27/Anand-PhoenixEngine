#pragma once
// The editor's current selection and in-place rename state.

#include <imgui.h>

class GameObject;

/// The selected GameObject (one at a time); `renaming` is the object whose name is being edited in the Hierarchy.
struct EditorSelection {
    GameObject* object = nullptr;
    GameObject* renaming = nullptr;
    char renameBuffer[256] = {};

    void clear(){ object = nullptr; }
    bool has() const { return object != nullptr; }
};
