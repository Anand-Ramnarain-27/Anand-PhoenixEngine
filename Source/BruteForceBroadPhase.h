#pragma once
#include "CollisionInterfaces.h"

class BruteForceBroadPhase : public IBroadPhase {
public:
    std::vector<CollisionPair> query(
        const std::vector<CollisionBody>& bodies) override;
    const char* getName() const override { return "Brute Force  O(N\xC2\xB2)"; }
};
