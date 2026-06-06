#pragma once
#include "Component.h"
#include "Globals.h"

// A decal component projects a texture onto surrounding scene geometry using
// deferred decal projection.  Place this component on any GameObject — the
// owner's transform determines position, rotation, and scale of the decal volume.
class ComponentDecal : public Component {
public:
    explicit ComponentDecal(GameObject* owner);
    ~ComponentDecal() override = default;

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Decal; }

    // Path to the albedo texture to project.  Empty = solid colour.
    std::string texturePath;

    // Tint / fallback colour (used when texturePath is empty or not yet loaded).
    Vector3 colour = Vector3(1.f, 1.f, 1.f);

    float opacity = 1.0f;
    bool enabled  = true;
};
