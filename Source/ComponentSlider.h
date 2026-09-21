#pragma once
#include "ComponentSelectable.h"
#include "ComponentTransform2D.h"
#include <algorithm>
#include <cmath>
#include <string>

// A draggable value. Drag the handle or click the track; with Tab focus the arrow keys nudge it.
// The ComponentTransform2D rect is the slider's full extent including room for the handle: the track is a thin
// strip through its middle and the handle travels along it.
class ComponentSlider : public ComponentSelectable {
public:
    enum class Direction {
        LeftToRight = 0,
        RightToLeft = 1,
        BottomToTop = 2,
        TopToBottom = 3,
    };

    explicit ComponentSlider(GameObject* owner) : ComponentSelectable(owner){}

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Slider; }

    float value = 0.5f;
    float minValue = 0.f;
    float maxValue = 1.f;
    bool wholeNumbers = false;
    Direction direction = Direction::LeftToRight;

    float trackThickness = 8.f;
    Vector2 handleSize = Vector2(20.f, 34.f);   // on-screen width, height

    Vector4 trackColor = Vector4(0.1f, 0.1f, 0.14f, 0.9f);
    Vector4 fillColor = Vector4(0.25f, 0.55f, 0.95f, 1.f);
    Vector4 handleColor = Vector4(1.f, 1.f, 1.f, 1.f);

    // Raised when the user changes the value (drag, click or arrow keys), not when game code sets it.
    UIEvent<float> onValueChanged;

    void clearListeners() override {
        ComponentSelectable::clearListeners();
        onValueChanged.clear();
    }

    // ---- value ----
    float lowest() const { return std::min(minValue, maxValue); }
    float highest() const { return std::max(minValue, maxValue); }

    float getNormalized() const {
        const float range = highest() - lowest();
        return range > 0.f ? std::clamp((value - lowest()) / range, 0.f, 1.f) : 0.f;
    }

    // Clamps (and rounds, for whole numbers). Returns true if the value changed.
    bool setValue(float v){
        if (wholeNumbers) v = std::round(v);
        v = std::clamp(v, lowest(), highest());
        if (v == value) return false;
        value = v;
        return true;
    }

    bool setNormalized(float t){ return setValue(lowest() + (highest() - lowest()) * std::clamp(t, 0.f, 1.f)); }

    // Amount an arrow key press moves the value.
    float keyStep() const { return wholeNumbers ? 1.f : (highest() - lowest()) * 0.05f; }

    // ---- geometry: shared by drawing and dragging so a value always maps to the same handle position ----
    bool isHorizontal() const { return direction == Direction::LeftToRight || direction == Direction::RightToLeft; }
    float handleLength() const { return isHorizontal() ? handleSize.x : handleSize.y; }

    // Range of the handle centre along the slider's main axis (x when horizontal, y when vertical).
    void travel(const UIRect& r, float& lo, float& hi) const {
        const float half = handleLength() * 0.5f;
        lo = (isHorizontal() ? r.min.x : r.min.y) + half;
        hi = (isHorizontal() ? r.max.x : r.max.y) - half;
    }

    // Main-axis coordinate of the handle centre for a 0..1 position.
    float positionAt(const UIRect& r, float t) const {
        float lo, hi;
        travel(r, lo, hi);
        switch (direction){
        case Direction::RightToLeft: return hi - (hi - lo) * t;
        case Direction::BottomToTop: return hi - (hi - lo) * t;   // screen Y grows downwards
        default:                     return lo + (hi - lo) * t;
        }
    }

    // 0..1 position for a point in the slider's local (unrotated) space.
    float normalizedAt(const UIRect& r, const Vector2& p) const {
        float lo, hi;
        travel(r, lo, hi);
        const float len = hi - lo;
        if (len <= 0.f) return 0.f;
        float t = 0.f;
        switch (direction){
        case Direction::LeftToRight: t = (p.x - lo) / len; break;
        case Direction::RightToLeft: t = (hi - p.x) / len; break;
        case Direction::BottomToTop: t = (hi - p.y) / len; break;
        case Direction::TopToBottom: t = (p.y - lo) / len; break;
        }
        return std::clamp(t, 0.f, 1.f);
    }
};
