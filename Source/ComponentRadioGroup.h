#pragma once
#include "Component.h"
#include "Globals.h"

// Marks a GameObject as the root of a mutually-exclusive set of options: every ComponentCheckBox under it
// (recursively, stopping at a nested RadioGroup, which owns its own members) belongs to this group. Selecting
// one unchecks the others. Purely organizational — it has no rect, no visuals and does not affect layout, so it
// works whether or not the GameObject also has a ComponentTransform2D.
class ComponentRadioGroup : public Component {
public:
    explicit ComponentRadioGroup(GameObject* owner) : Component(owner){}

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::RadioGroup; }

    // false (default): the group always keeps exactly one option selected once one has been chosen — clicking
    // the already-selected option does nothing. true: clicking the selected option deselects it, so the group
    // can end up with nothing chosen, like a normal CheckBox with exclusivity added on top.
    bool allowSwitchOff = false;
};
