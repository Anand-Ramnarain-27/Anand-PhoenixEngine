#pragma once
#include "ComponentSelectable.h"
#include <string>

// A toggle. The ComponentTransform2D rect is the whole clickable row: the box sits at its left edge and a child
// Label placed over the rest of the row is part of the click target, so clicking the text toggles it too.
class ComponentCheckBox : public ComponentSelectable {
public:
    explicit ComponentCheckBox(GameObject* owner) : ComponentSelectable(owner){}

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::CheckBox; }

    bool checked = false;

    float boxSize = 0.f;   // side of the box in canvas units; 0 = the row height
    Vector4 boxColor = Vector4(0.14f, 0.15f, 0.2f, 1.f);
    Vector4 checkColor = Vector4(0.35f, 0.85f, 0.45f, 1.f);
    std::string checkTexture;   // optional mark; a flat inset square when empty

    // Raised when the user toggles it (not when game code calls SetChecked).
    UIEvent<bool> onValueChanged;

    void clearListeners() override {
        ComponentSelectable::clearListeners();
        onValueChanged.clear();
    }
};
