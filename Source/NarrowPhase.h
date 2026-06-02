#pragma once
#include "CollisionTypes.h"
#include <vector>

// Exact OBB–OBB intersection test using the Separating Axis Theorem (15 axes).
// Returns a ContactPoint for every confirmed collision, including the contact
// position (centroid approximation), separating normal, and penetration depth.
class NarrowPhase {
public:
    std::vector<ContactPoint> test(
        const std::vector<CollisionPair>& pairs,
        const std::vector<CollisionBody>& bodies);
};
