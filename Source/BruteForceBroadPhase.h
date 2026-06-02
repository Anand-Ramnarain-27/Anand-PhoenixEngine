#pragma once
#include "IBroadPhase.h"

// O(N²) broad phase: tests every pair of world-space AABBs.
// Swap this for a spatial partition (BVH, uniform grid, etc.) by implementing
// IBroadPhase and calling CollisionSystem::setBroadPhase() — nothing else changes.
class BruteForceBroadPhase : public IBroadPhase {
public:
    std::vector<CollisionPair> query(
        const std::vector<CollisionBody>& bodies) override;
};
