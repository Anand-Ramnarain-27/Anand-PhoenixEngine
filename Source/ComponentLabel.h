#pragma once
#include "Component.h"
#include "Globals.h"

// Draws text inside the widget's ComponentTransform2D rect using a baked .spritefont.
class ComponentLabel : public Component {
public:
    enum class HAlign { Left = 0, Center = 1, Right = 2 };
    enum class VAlign { Top = 0, Middle = 1, Bottom = 2 };

    explicit ComponentLabel(GameObject* owner) : Component(owner){}

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Label; }

    bool enabled = true;
    std::string text = "Label";
    std::string fontName = "UIFont";
    float fontSize = 32.f;   // line height in canvas units
    Vector4 color = Vector4(1.f, 1.f, 1.f, 1.f);
    HAlign hAlign = HAlign::Center;
    VAlign vAlign = VAlign::Middle;
};
