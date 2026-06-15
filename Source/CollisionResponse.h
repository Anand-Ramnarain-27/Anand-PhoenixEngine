#pragma once
#include "CollisionInterfaces.h"
#include <vector>

class CollisionResponse {
public:
    void solve(const std::vector<ContactPoint>& contacts, float dt);
};
