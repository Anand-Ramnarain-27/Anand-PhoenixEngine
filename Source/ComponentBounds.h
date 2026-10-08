#pragma once
// Collision bounds for a GameObject: an AABB, a sphere or an OBB fitted to its mesh.

#include "Component.h"
#include "BoundingVolume.h"

/// Selects the bounding volume the collision pipeline uses for this object.
class ComponentBounds final : public Component {
public:
    explicit ComponentBounds(GameObject* owner);

    BVType bvType = BVType::AABB;

    /// Sphere radius; < 0 fits it to the mesh.
    float radiusOverride = -1.f;

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Bounds; }
};
