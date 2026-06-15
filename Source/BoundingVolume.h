#pragma once
#include "Globals.h"
#include <cmath>
#include <cfloat>

struct AABB {
    Vector3 min = { FLT_MAX, FLT_MAX, FLT_MAX };
    Vector3 max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    bool isValid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }

    bool intersects(const AABB& other) const{
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    void update(const Vector3& localMin, const Vector3& localMax, const Matrix& world){
        const Vector3& mn = localMin;
        const Vector3& mx = localMax;
        Vector3 corners[8] = {
            {mn.x,mn.y,mn.z}, {mx.x,mn.y,mn.z}, {mn.x,mx.y,mn.z}, {mx.x,mx.y,mn.z},
            {mn.x,mn.y,mx.z}, {mx.x,mn.y,mx.z}, {mn.x,mx.y,mx.z}, {mx.x,mx.y,mx.z},
        };
        min = Vector3( FLT_MAX, FLT_MAX, FLT_MAX);
        max = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (const auto& c : corners){
            Vector3 wc = Vector3::Transform(c, world);
            min = Vector3::Min(min, wc);
            max = Vector3::Max(max, wc);
        }
    }

    void updateFromPositionScale(const Vector3& position, const Vector3& scale){
        Vector3 half = scale * 0.5f;
        min = position - half;
        max = position + half;
    }
};

enum class BVType { AABB, Sphere };

struct Sphere {
    Vector3 center;
    float radius = 0.f;

    bool isValid() const { return radius > 0.f; }

    bool intersects(const Sphere& other) const{
        float rSum = radius + other.radius;
        return (center - other.center).LengthSquared() < rSum * rSum;
    }

    bool intersects(const AABB& box) const{
        float cx = center.x < box.min.x ? box.min.x : (center.x > box.max.x ? box.max.x : center.x);
        float cy = center.y < box.min.y ? box.min.y : (center.y > box.max.y ? box.max.y : center.y);
        float cz = center.z < box.min.z ? box.min.z : (center.z > box.max.z ? box.max.z : center.z);
        float dx = center.x - cx, dy = center.y - cy, dz = center.z - cz;
        return dx*dx + dy*dy + dz*dz < radius * radius;
    }

    AABB toAABB() const{
        return { center - Vector3(radius, radius, radius),
                 center + Vector3(radius, radius, radius) };
    }
};
