#pragma once
#include "CollisionTypes.h"
#include <vector>

class IMidPhase {
public:
    virtual ~IMidPhase() = default;
    virtual std::vector<CollisionPair> filter(
        std::vector<CollisionPair> candidates,
        const std::vector<CollisionBody>& bodies) = 0;
};

class PassthroughMidPhase : public IMidPhase {
public:
    std::vector<CollisionPair> filter(
        std::vector<CollisionPair> candidates,
        const std::vector<CollisionBody>& ) override {
        return candidates;
    }
};
