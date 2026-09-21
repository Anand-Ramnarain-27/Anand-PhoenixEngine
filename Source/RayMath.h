#pragma once
#include "Globals.h"
#include <cfloat>

// Shared ray-vs-shape math, used by MousePicker and CollisionSystem::Raycast.
namespace RayMath {
    // Namespaced, not global - DirectX::SimpleMath already has its own ::Ray.
    struct Ray {
        Vector3 origin;
        Vector3 direction; // expected normalized
    };

    // Returns hit distance in [0, maxDist], or FLT_MAX if there's no hit.
    float RayVsAABB(const Ray& ray, const Vector3& mn, const Vector3& mx, float maxDist = FLT_MAX);
    float RayVsSphere(const Ray& ray, const Vector3& center, float radius, float maxDist = FLT_MAX);

    // axes/halves as on CollisionBody. outNormal, if given, gets the hit face's world-space normal.
    float RayVsOBB(const Ray& ray, const Vector3& center, const Vector3 axes[3], const float halves[3],
                   float maxDist = FLT_MAX, Vector3* outNormal = nullptr);

    float RayVsTriangle(const Ray& ray, const Vector3& v0, const Vector3& v1, const Vector3& v2);
}
