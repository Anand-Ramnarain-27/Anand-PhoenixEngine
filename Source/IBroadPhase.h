#pragma once
#include "CollisionTypes.h"
#include <vector>

// Abstract broad-phase interface. Implementations receive the flat list of
// CollisionBody objects built by CollisionSystem and return candidate pairs
// (indices into that array) that may be colliding. Replace by swapping the
// concrete implementation passed to CollisionSystem::setBroadPhase().
class IBroadPhase {
public:
    virtual ~IBroadPhase() = default;
    virtual std::vector<CollisionPair> query(
        const std::vector<CollisionBody>& bodies) = 0;
};
