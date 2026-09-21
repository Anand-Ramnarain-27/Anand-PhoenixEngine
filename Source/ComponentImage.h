#pragma once
#include "Component.h"
#include "Globals.h"

// Draws a texture (or a flat colour when no texture is set) filling the widget's ComponentTransform2D rect.
class ComponentImage : public Component {
public:
    explicit ComponentImage(GameObject* owner) : Component(owner){}

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Image; }

    bool enabled = true;

    // When set, this image blocks the pointer from reaching widgets drawn behind it.
    bool raycastTarget = true;
    std::string texturePath;
    Vector4 tint = Vector4(1.f, 1.f, 1.f, 1.f);

    // Optional sub-rectangle of the texture in texels (x, y, width, height); drawn whole when disabled.
    bool useSourceRect = false;
    Vector4 sourceRect = Vector4(0.f, 0.f, 0.f, 0.f);
};
