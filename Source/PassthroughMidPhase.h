#pragma once
#include "IMidPhase.h"

// Identity mid-phase: returns all broad-phase candidates unchanged.
// Replace with mesh-level or convex-hull tests to reduce narrow-phase work.
class PassthroughMidPhase : public IMidPhase {
public:
    std::vector<CollisionPair> filter(
        std::vector<CollisionPair>        candidates,
        const std::vector<CollisionBody>& /*bodies*/) override
    {
        return candidates;
    }
};
