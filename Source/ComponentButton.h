#pragma once
#include "ComponentSelectable.h"
#include <string>

// A clickable widget. It reacts to the pointer over its ComponentTransform2D rect (or over a child Image that
// blocks input), tints/swaps the sibling ComponentImage by state, and raises events to game code.
class ComponentButton : public ComponentSelectable {
public:
    explicit ComponentButton(GameObject* owner) : ComponentSelectable(owner){}

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Button; }

    // Multiplied into the sibling Image's tint per state.
    Vector4 normalColor = Vector4(1.f, 1.f, 1.f, 1.f);
    Vector4 hoverColor = Vector4(0.88f, 0.88f, 0.88f, 1.f);
    Vector4 pressedColor = Vector4(0.7f, 0.7f, 0.7f, 1.f);
    Vector4 disabledColor = Vector4(0.5f, 0.5f, 0.5f, 0.6f);

    // Optional sprite swap; an empty path keeps the Image's own texture.
    std::string hoverTexture;
    std::string pressedTexture;
    std::string disabledTexture;

    const Vector4& currentColor() const {
        switch (state){
        case State::Hovered:  return hoverColor;
        case State::Pressed:  return pressedColor;
        case State::Disabled: return disabledColor;
        default:              return normalColor;
        }
    }

    // The texture to draw for the current state, falling back to `base`.
    const std::string& currentTexture(const std::string& base) const {
        const std::string* swap = nullptr;
        switch (state){
        case State::Hovered:  swap = &hoverTexture; break;
        case State::Pressed:  swap = &pressedTexture; break;
        case State::Disabled: swap = &disabledTexture; break;
        default: break;
        }
        return (swap && !swap->empty()) ? *swap : base;
    }
};
