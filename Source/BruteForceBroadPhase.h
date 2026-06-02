#pragma once
#include "IBroadPhase.h"

// O(N²) broad phase: tests every pair of world-space AABBs.
// Kept as a reference/comparison implementation alongside UniformGridBroadPhase.
class BruteForceBroadPhase : public IBroadPhase {
public:
    std::vector<CollisionPair> query(
        const std::vector<CollisionBody>& bodies) override;
    const char* getName() const override { return "Brute Force  O(N\xC2\xB2)"; }
};
