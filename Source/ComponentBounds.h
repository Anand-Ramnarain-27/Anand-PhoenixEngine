#pragma once
#include "Component.h"
#include "BoundingVolume.h"

class ComponentBounds final : public Component {
public:
    explicit ComponentBounds(GameObject* owner);

    BVType bvType = BVType::AABB;

    float radiusOverride = -1.f;

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Bounds; }
};
