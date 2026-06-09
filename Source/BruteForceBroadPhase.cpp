#include "Globals.h"
#include "BruteForceBroadPhase.h"

std::vector<CollisionPair> BruteForceBroadPhase::query(
    const std::vector<CollisionBody>& bodies){
    std::vector<CollisionPair> pairs;
    const uint32_t n = static_cast<uint32_t>(bodies.size());
    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = i + 1; j < n; ++j) {
            if (bodies[i].worldAABB.intersects(bodies[j].worldAABB))
                pairs.push_back({ i, j });
        }
    }
    return pairs;
}
