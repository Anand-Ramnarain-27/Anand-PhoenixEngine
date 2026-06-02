#include "Globals.h"
#include "NarrowPhase.h"
#include <cmath>
#include <cfloat>

// ---------------------------------------------------------------------------
// OBB–OBB Separating Axis Theorem
//
// Follows the classic formulation from Ericson, "Real-Time Collision Detection"
// (Chapter 4). All 15 candidate separating axes are tested:
//   • 3 face normals of OBB A
//   • 3 face normals of OBB B
//   • 9 edge–edge cross products (A.ax[i] × B.ax[j])
//
// Returns true if the OBBs overlap and fills `contact` with:
//   normal  — minimum-separation direction pointing from B toward A
//   depth   — penetration depth along that axis
//   point   — centroid midpoint (conservative approximation for demos)
// ---------------------------------------------------------------------------

static bool obbVsOBB(const CollisionBody& ba, const CollisionBody& bb,
                     ContactPoint& contact)
{
    const Vector3  T  = bb.obbCenter - ba.obbCenter;
    const Vector3* uA = ba.obbAxes;
    const float*   eA = ba.obbHalves;
    const Vector3* uB = bb.obbAxes;
    const float*   eB = bb.obbHalves;

    // Project the full OBB onto a normalised axis and return the radius.
    auto project = [](const Vector3* axes, const float* halves, const Vector3& L) -> float {
        return halves[0] * fabsf(axes[0].Dot(L))
             + halves[1] * fabsf(axes[1].Dot(L))
             + halves[2] * fabsf(axes[2].Dot(L));
    };

    float   minPen  = FLT_MAX;
    Vector3 bestNrm;

    // Test a candidate axis L (already normalised or zero-length).
    // Returns false immediately if it is a separating axis.
    auto testAxis = [&](Vector3 L) -> bool {
        float len = L.Length();
        if (len < 1e-6f) return true; // degenerate cross product — skip
        L /= len;
        float ra  = project(uA, eA, L);
        float rb  = project(uB, eB, L);
        float pen = ra + rb - fabsf(T.Dot(L));
        if (pen <= 0.f) return false; // separating axis found
        if (pen < minPen) {
            minPen  = pen;
            // Flip so normal points from B toward A.
            bestNrm = (T.Dot(L) >= 0.f) ? -L : L;
        }
        return true;
    };

    // --- A face normals ---
    for (int i = 0; i < 3; ++i)
        if (!testAxis(uA[i])) return false;

    // --- B face normals ---
    for (int j = 0; j < 3; ++j)
        if (!testAxis(uB[j])) return false;

    // --- Edge–edge cross products ---
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (!testAxis(uA[i].Cross(uB[j]))) return false;

    // No separating axis found — collision.
    contact.a      = ba.go;
    contact.b      = bb.go;
    contact.normal = bestNrm;
    contact.depth  = minPen;
    contact.point  = (ba.obbCenter + bb.obbCenter) * 0.5f;
    return true;
}

// ---------------------------------------------------------------------------

std::vector<ContactPoint> NarrowPhase::test(
    const std::vector<CollisionPair>& pairs,
    const std::vector<CollisionBody>& bodies)
{
    std::vector<ContactPoint> results;
    results.reserve(pairs.size());

    for (const auto& p : pairs) {
        ContactPoint cp;
        if (obbVsOBB(bodies[p.a], bodies[p.b], cp))
            results.push_back(cp);
    }
    return results;
}
