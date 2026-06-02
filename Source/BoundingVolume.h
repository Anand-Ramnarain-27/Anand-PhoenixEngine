#pragma once
#include "Globals.h"
#include "AABB.h"
#include <cmath>

// Which bounding volume shape a CollisionBody uses in the narrow phase.
// The broad phase always uses a world-space AABB regardless of shape.
enum class BVType { AABB, Sphere };

// World-space sphere bounding volume.
struct Sphere {
    Vector3 center;
    float   radius = 0.f;

    bool isValid() const { return radius > 0.f; }

    // Sphere vs Sphere overlap test.
    bool intersects(const Sphere& other) const {
        float rSum = radius + other.radius;
        return (center - other.center).LengthSquared() < rSum * rSum;
    }

    // Sphere vs AABB overlap test — closest-point-on-box approach.
    bool intersects(const AABB& box) const {
        // Closest point on AABB to sphere centre (component-wise clamp).
        float cx = center.x < box.min.x ? box.min.x : (center.x > box.max.x ? box.max.x : center.x);
        float cy = center.y < box.min.y ? box.min.y : (center.y > box.max.y ? box.max.y : center.y);
        float cz = center.z < box.min.z ? box.min.z : (center.z > box.max.z ? box.max.z : center.z);
        float dx = center.x - cx, dy = center.y - cy, dz = center.z - cz;
        return dx*dx + dy*dy + dz*dz < radius * radius;
    }

    // Tight AABB enclosing this sphere — used as the broad-phase proxy.
    AABB toAABB() const {
        return { center - Vector3(radius, radius, radius),
                 center + Vector3(radius, radius, radius) };
    }
};
