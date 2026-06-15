#pragma once
#include "CollisionTypes.h"
#include <vector>

class NarrowPhase {
public:
    std::vector<ContactPoint> test(
        const std::vector<CollisionPair>& pairs,
        const std::vector<CollisionBody>& bodies);
};
