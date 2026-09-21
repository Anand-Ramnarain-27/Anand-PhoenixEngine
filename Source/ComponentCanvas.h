#pragma once
#include "Component.h"
#include "Globals.h"

// Root of a UI hierarchy. Every ComponentTransform2D underneath is laid out inside the canvas rect,
// which is the screen size divided by the canvas scale factor.
class ComponentCanvas : public Component {
public:
    enum class ScaleMode {
        ConstantPixelSize = 0,
        ScaleWithScreenSize = 1,
    };

    explicit ComponentCanvas(GameObject* owner) : Component(owner){}

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Canvas; }

    bool enabled = true;
    int sortOrder = 0;
    ScaleMode scaleMode = ScaleMode::ScaleWithScreenSize;
    Vector2 referenceResolution = Vector2(1920.f, 1080.f);
    float matchWidthOrHeight = 0.5f;

    // Canvas units -> screen pixels for a given screen size.
    float getScaleFactor(float screenW, float screenH) const;
};
