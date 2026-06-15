#pragma once
#include "CollisionTypes.h"
#include <vector>

class IBroadPhase {
public:
    virtual ~IBroadPhase() = default;

    virtual std::vector<CollisionPair> query(
        const std::vector<CollisionBody>& bodies) = 0;

    virtual const char* getName() const = 0;

    virtual void drawDebug(){}
};
