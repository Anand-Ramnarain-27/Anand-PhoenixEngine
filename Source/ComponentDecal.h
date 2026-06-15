#pragma once
#include "Component.h"
#include "Globals.h"

class ComponentDecal : public Component {
public:
    explicit ComponentDecal(GameObject* owner);
    ~ComponentDecal() override = default;

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Decal; }

    std::string texturePath;

    Vector3 colour = Vector3(1.f, 1.f, 1.f);

    float opacity = 1.0f;
    bool enabled = true;
};
