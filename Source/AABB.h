#pragma once
#include "Globals.h"

// Axis-aligned bounding box in world space.
// min/max are invalid (FLT_MAX / -FLT_MAX) until update() is called.
struct AABB {
    Vector3 min = { FLT_MAX,  FLT_MAX,  FLT_MAX };
    Vector3 max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    bool isValid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }

    // SAT overlap test — returns true if the two AABBs share any volume.
    bool intersects(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    // Recompute world-space AABB by transforming the 8 corners of a local box
    // through a world matrix. Matches ComponentMesh::getWorldAABB().
    void update(const Vector3& localMin, const Vector3& localMax, const Matrix& world) {
        const Vector3& mn = localMin;
        const Vector3& mx = localMax;
        Vector3 corners[8] = {
            {mn.x,mn.y,mn.z}, {mx.x,mn.y,mn.z}, {mn.x,mx.y,mn.z}, {mx.x,mx.y,mn.z},
            {mn.x,mn.y,mx.z}, {mx.x,mn.y,mx.z}, {mn.x,mx.y,mx.z}, {mx.x,mx.y,mx.z},
        };
        min = Vector3( FLT_MAX,  FLT_MAX,  FLT_MAX);
        max = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (const auto& c : corners) {
            Vector3 wc = Vector3::Transform(c, world);
            min = Vector3::Min(min, wc);
            max = Vector3::Max(max, wc);
        }
    }

    // Build from a GameObject's position and scale, treating a unit cube as the
    // bounding shape. Useful for non-mesh objects (lights, cameras, empties).
    void updateFromPositionScale(const Vector3& position, const Vector3& scale) {
        Vector3 half = scale * 0.5f;
        min = position - half;
        max = position + half;
    }
};
