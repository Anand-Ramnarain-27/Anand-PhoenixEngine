#pragma once
#include <imgui.h>

class GameObject;

struct EditorSelection
{
    GameObject* object = nullptr;
    GameObject* renaming = nullptr;
    char        renameBuffer[256] = {};

    void clear() { object = nullptr; }
    bool has() const { return object != nullptr; }
};