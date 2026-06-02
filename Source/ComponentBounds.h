#pragma once
#include "Component.h"
#include "BoundingVolume.h"

// Controls which bounding-volume shape is used for a GameObject in the
// collision pipeline.  Without this component the pipeline defaults to AABB.
//
// BVType::AABB   — oriented bounding box derived from the mesh local AABB
//                  (same behaviour as before this component existed).
// BVType::Sphere — circumscribed sphere whose radius is either derived
//                  automatically from the mesh AABB half-diagonal, or
//                  overridden via radiusOverride.
class ComponentBounds final : public Component {
public:
    explicit ComponentBounds(GameObject* owner);

    BVType bvType        = BVType::AABB;

    // When >= 0: use this world-space radius (before object scale is applied)
    // instead of the value derived from the mesh.  Set to -1 for auto.
    float  radiusOverride = -1.f;

    // ---- Component interface ----
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Bounds; }
};
