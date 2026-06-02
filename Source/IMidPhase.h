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
