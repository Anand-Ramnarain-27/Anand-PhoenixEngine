#pragma once
#include "Globals.h"
#include <cfloat>

// Shared ray-vs-shape math - no engine dependencies beyond Vector3/Matrix.
// Used by both MousePicker (editor click-selection) and CollisionSystem's
// Raycast (gameplay/AI line-of-sight), rather than each keeping its own copy.
//
// Ray lives inside this namespace (not at global scope) because Globals.h
// pulls in `using namespace DirectX::SimpleMath`, which already defines its
// own ::Ray - a global one here would be ambiguous at every call site.
namespace RayMath {
    struct Ray {
        Vector3 origin;
        Vector3 direction; // expected normalized
    };

    // All *Vs* functions return the hit distance along the ray within [0, maxDist],
    // or FLT_MAX if there is no hit.
    float RayVsAABB(const Ray& ray, const Vector3& mn, const Vector3& mx, float maxDist = FLT_MAX);
    float RayVsSphere(const Ray& ray, const Vector3& center, float radius, float maxDist = FLT_MAX);

    // axes/halves as stored on CollisionBody (world-space orthonormal axes + half-extents).
    // outNormal, if non-null, is filled with the world-space normal of the hit face.
    float RayVsOBB(const Ray& ray, const Vector3& center, const Vector3 axes[3], const float halves[3],
                   float maxDist = FLT_MAX, Vector3* outNormal = nullptr);

    float RayVsTriangle(const Ray& ray, const Vector3& v0, const Vector3& v1, const Vector3& v2);
}
