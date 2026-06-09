#pragma once
#include "Component.h"
#include "Globals.h"

// A billboard component renders a camera-facing quad at the owner's transform
// position — used for transparent effects like fire, smoke, sprites, etc.
// Optionally animates through a grid sprite-sheet ("flipbook" animation).
class ComponentBillboard : public Component {
public:
    enum class Alignment {
        Screen = 0, // N parallel to screen normal — locked to the camera, used for HUD-like 2D fx
        World = 1, // N points from billboard to camera, U = world up — "look-at" billboard
        Axial = 2, // U fixed (world up), billboard rotates only around that axis — trees/cylinders
    };

    explicit ComponentBillboard(GameObject* owner);
    ~ComponentBillboard() override = default;

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Billboard; }

    // Path to the sprite-sheet / texture to display. Empty = fallback white quad.
    std::string texturePath;

    Alignment alignment = Alignment::Screen;

    // World-space size of the quad (width, height).
    Vector2 size = Vector2(1.f, 1.f);

    // Tint multiplied with the sampled texture colour (alpha included).
    Vector4 tint = Vector4(1.f, 1.f, 1.f, 1.f);

    // Sprite-sheet grid. 1x1 = no animation, the whole texture is shown.
    int sheetColumns = 1;
    int sheetRows = 1;

    // Frames played per second. 0 = static (first tile only).
    float framesPerSecond = 0.f;
    bool loop = true;

    bool enabled = true;

    // Current (possibly fractional) animation frame — advanced by update().
    // Fractional part drives interpolation between consecutive tiles.
    float getCurrentFrame() const { return m_currentFrame; }

private:
    float m_currentFrame = 0.f;
};
