#pragma once
#include "Component.h"
#include "Globals.h"
#include <algorithm>
#include <string>

// A bar that fills in proportion to a value: health, loading, cooldowns. It draws a background over its whole
// ComponentTransform2D rect and a fill over the part of it the value covers. Put a child Label on top for text.
//
// Everything a script needs is plain data or inline code, so GameScript.dll can drive it without the renderer.
class ComponentProgressBar : public Component {
public:
    enum class FillDirection {
        LeftToRight = 0,
        RightToLeft = 1,
        BottomToTop = 2,
        TopToBottom = 3,
    };

    explicit ComponentProgressBar(GameObject* owner) : Component(owner){}

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::ProgressBar; }

    bool enabled = true;

    float value = 1.f;
    float minValue = 0.f;
    float maxValue = 1.f;
    FillDirection direction = FillDirection::LeftToRight;

    Vector4 backgroundColor = Vector4(0.08f, 0.08f, 0.1f, 0.85f);
    Vector4 fillColor = Vector4(0.25f, 0.8f, 0.35f, 1.f);

    // Optional textures. A fill texture is cropped to the filled part rather than squashed into it.
    std::string backgroundTexture;
    std::string fillTexture;

    // `value` mapped onto 0..1 across [minValue, maxValue].
    float getNormalized() const {
        const float range = maxValue - minValue;
        return range > 0.f ? std::clamp((value - minValue) / range, 0.f, 1.f) : 0.f;
    }
};
