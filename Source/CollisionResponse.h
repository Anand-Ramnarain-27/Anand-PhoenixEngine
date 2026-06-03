#pragma once
#include "CollisionTypes.h"
#include <vector>

// Resolves confirmed contacts produced by NarrowPhase in two passes:
//
//   1. Position correction — pushes overlapping objects apart along the
//      contact normal, weighted by inverse mass so heavy objects move less.
//
//   2. Velocity impulse — applies an instantaneous change in velocity using
//      momentum conservation with a per-object restitution coefficient.
//
// Only GameObjects that carry a ComponentRigidbody participate.  Objects
// without the component (or with mass == 0 / isStatic == true) act as
// immovable static obstacles.
class CollisionResponse {
public:
    void solve(const std::vector<ContactPoint>& contacts, float dt);

    // Tunable solver parameters — editable from Collision Debug / Physics Settings.
    float correctionPercent = 0.8f;   // fraction of penetration corrected per frame
    float correctionSlop    = 0.005f; // min penetration before correction kicks in (m)
};
