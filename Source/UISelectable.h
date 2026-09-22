#pragma once
#include "GameObject.h"
#include "ComponentButton.h"
#include "ComponentCheckBox.h"
#include "ComponentSlider.h"
#include "ComponentInputBox.h"

// The interactive part of whichever widget component `go` has, or null.
inline ComponentSelectable* selectableOf(GameObject* go){
    if (!go) return nullptr;
    if (auto* b = go->getComponent<ComponentButton>()) return b;
    if (auto* c = go->getComponent<ComponentCheckBox>()) return c;
    if (auto* s = go->getComponent<ComponentSlider>()) return s;
    if (auto* i = go->getComponent<ComponentInputBox>()) return i;
    return nullptr;
}
