#pragma once
#include "CollisionTypes.h"
#include <vector>

// Abstract broad-phase interface. Implementations receive the flat list of
// CollisionBody objects built by CollisionSystem and return candidate pairs
// (indices into that array) that may be colliding. Replace by swapping the
// concrete implementation passed to CollisionSystem::setBroadPhase() — nothing
// else in the pipeline needs to change.
class IBroadPhase {
public:
    virtual ~IBroadPhase() = default;

    virtual std::vector<CollisionPair> query(
        const std::vector<CollisionBody>& bodies) = 0;

    // Human-readable name shown in the Collision Debug panel.
    virtual const char* getName() const = 0;

    // Called each frame (inside editorExtras) to draw a visualisation of the
    // spatial structure.  Default is a no-op so existing implementations don't
    // need to change.
    virtual void drawDebug() {}
};
