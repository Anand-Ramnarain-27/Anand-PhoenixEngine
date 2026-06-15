#include "Globals.h"
#include "NarrowPhase.h"
#include <cmath>
#include <cfloat>
#include <algorithm>

static bool obbVsOBB(const CollisionBody& ba, const CollisionBody& bb,
                     ContactPoint& contact){
    const Vector3 T = bb.obbCenter - ba.obbCenter;
    const Vector3* uA = ba.obbAxes;
    const float* eA = ba.obbHalves;
    const Vector3* uB = bb.obbAxes;
    const float* eB = bb.obbHalves;

    auto project = [](const Vector3* axes, const float* halves, const Vector3& L) -> float {
        return halves[0] * fabsf(axes[0].Dot(L))
             + halves[1] * fabsf(axes[1].Dot(L))
             + halves[2] * fabsf(axes[2].Dot(L));
    };

    float minPen = FLT_MAX;
    Vector3 bestNrm;

    auto testAxis = [&](Vector3 L) -> bool {
        float len = L.Length();
        if (len < 1e-6f) return true;
        L /= len;
        float ra = project(uA, eA, L);
        float rb = project(uB, eB, L);
        float pen = ra + rb - fabsf(T.Dot(L));
        if (pen <= 0.f) return false;
        if (pen < minPen){ minPen = pen; bestNrm = (T.Dot(L) >= 0.f) ? -L : L; }
        return true;
    };

    for (int i = 0; i < 3; ++i) if (!testAxis(uA[i])) return false;
    for (int j = 0; j < 3; ++j) if (!testAxis(uB[j])) return false;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (!testAxis(uA[i].Cross(uB[j]))) return false;

    contact.a = ba.go;
    contact.b = bb.go;
    contact.normal = bestNrm;
    contact.depth = minPen;
    contact.point = (ba.obbCenter + bb.obbCenter) * 0.5f;
    return true;
}

static bool sphereVsSphere(const CollisionBody& ba, const CollisionBody& bb,
                            ContactPoint& contact){
    Vector3 diff = ba.sphereCenter - bb.sphereCenter;
    float distSq = diff.LengthSquared();
    float rSum = ba.sphereRadius + bb.sphereRadius;

    if (distSq >= rSum * rSum) return false;

    float dist = sqrtf(distSq);

    contact.a = ba.go;
    contact.b = bb.go;

    if (dist < 1e-6f){
        contact.normal = Vector3(0.f, 1.f, 0.f);
        contact.depth = rSum;
        contact.point = ba.sphereCenter;
    } else {
        contact.normal = diff / dist;
        contact.depth = rSum - dist;
        contact.point = bb.sphereCenter + contact.normal * bb.sphereRadius;
    }
    return true;
}

static bool sphereVsOBB(const CollisionBody& bSphere, const CollisionBody& bOBB,
                         ContactPoint& contact){
    Vector3 d = bSphere.sphereCenter - bOBB.obbCenter;

    Vector3 closest = bOBB.obbCenter;
    for (int i = 0; i < 3; ++i){
        float proj = d.Dot(bOBB.obbAxes[i]);
        float clamped = std::clamp(proj, -bOBB.obbHalves[i], bOBB.obbHalves[i]);
        closest += bOBB.obbAxes[i] * clamped;
    }

    Vector3 diff = bSphere.sphereCenter - closest;
    float distSq = diff.LengthSquared();

    if (distSq >= bSphere.sphereRadius * bSphere.sphereRadius) return false;

    contact.a = bSphere.go;
    contact.b = bOBB.go;

    float dist = sqrtf(distSq);
    if (dist < 1e-6f){
        float minPen = FLT_MAX;
        Vector3 bestNrm;
        for (int i = 0; i < 3; ++i){
            float q = d.Dot(bOBB.obbAxes[i]);
            float pen = bOBB.obbHalves[i] - fabsf(q) + bSphere.sphereRadius;
            if (pen < minPen){
                minPen = pen;
                bestNrm = (q >= 0.f) ? bOBB.obbAxes[i] : -bOBB.obbAxes[i];
            }
        }
        contact.normal = bestNrm;
        contact.depth = minPen;
        contact.point = closest;
    } else {
        contact.normal = diff / dist;
        contact.depth = bSphere.sphereRadius - dist;
        contact.point = closest;
    }
    return true;
}


std::vector<ContactPoint> NarrowPhase::test(
    const std::vector<CollisionPair>& pairs,
    const std::vector<CollisionBody>& bodies){
    std::vector<ContactPoint> results;
    results.reserve(pairs.size());

    for (const auto& p : pairs){
        const CollisionBody& ba = bodies[p.a];
        const CollisionBody& bb = bodies[p.b];
        ContactPoint cp;
        bool hit = false;

        const bool aIsSphere = (ba.bvType == BVType::Sphere);
        const bool bIsSphere = (bb.bvType == BVType::Sphere);

        if (!aIsSphere && !bIsSphere){
            hit = obbVsOBB(ba, bb, cp);
        } else if (aIsSphere && bIsSphere){
            hit = sphereVsSphere(ba, bb, cp);
        } else if (aIsSphere && !bIsSphere){
            hit = sphereVsOBB(ba, bb, cp);
        } else {
            hit = sphereVsOBB(bb, ba, cp);
            if (hit){
                std::swap(cp.a, cp.b);
                cp.normal = -cp.normal;
            }
        }

        if (hit) results.push_back(cp);
    }
    return results;
}
