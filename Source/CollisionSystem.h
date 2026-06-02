#pragma once
#include "CollisionTypes.h"
#include "IBroadPhase.h"
#include "IMidPhase.h"
#include "NarrowPhase.h"
#include <memory>

class ModuleScene;

// Owns and sequences the three pipeline stages each frame.
// Call setBroadPhase() to hot-swap the broad-phase implementation without
// touching MidPhase or NarrowPhase.
class CollisionSystem {
public:
    CollisionSystem();
    ~CollisionSystem() = default;

    // Replace the broad-phase. The new object takes effect on the next run().
    void setBroadPhase(std::unique_ptr<IBroadPhase> bp);

    // Gather objects from the scene, run the full pipeline, cache results.
    void run(ModuleScene* scene);

    const CollisionResults& getResults() const { return m_results; }

private:
    static std::vector<CollisionBody> gatherBodies(ModuleScene* scene);

    std::unique_ptr<IBroadPhase> m_broadPhase;
    std::unique_ptr<IMidPhase>   m_midPhase;
    NarrowPhase                  m_narrowPhase;
    CollisionResults             m_results;
};
