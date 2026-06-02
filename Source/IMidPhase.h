#pragma once
#include "CollisionTypes.h"
#include <vector>

// Abstract mid-phase interface. Receives the candidate pairs from BroadPhase and
// the full body list, and returns a (potentially smaller) filtered set. Replace
// by injecting a different concrete implementation into CollisionSystem.
class IMidPhase {
public:
    virtual ~IMidPhase() = default;
    virtual std::vector<CollisionPair> filter(
        std::vector<CollisionPair>        candidates,
        const std::vector<CollisionBody>& bodies) = 0;
};

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
