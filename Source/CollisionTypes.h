#pragma once
#include "Globals.h"
#include "AABB.h"
#include <vector>
#include <cstdint>

class GameObject;

// Pre-built per-object collision representation gathered once at the start of each
// pipeline run. BroadPhase uses the world AABB; NarrowPhase uses the OBB.
struct CollisionBody {
    GameObject* go        = nullptr;
    AABB        worldAABB;          // axis-aligned, rebuilt every frame

    // Oriented bounding box — axes are unit vectors extracted from the world matrix.
    // Half-extents are scaled by the object's world-space scale.
    Vector3 obbCenter;
    Vector3 obbAxes[3];   // unit vectors
    float   obbHalves[3]; // half-extents along each axis
};

// A pair of indices into the CollisionBody array produced by BroadPhase / MidPhase.
struct CollisionPair {
    uint32_t a;
    uint32_t b;
};

// Filled by NarrowPhase for every confirmed contact.
struct ContactPoint {
    GameObject* a       = nullptr;
    GameObject* b       = nullptr;
    Vector3     point;            // world-space contact point (centroid approximation)
    Vector3     normal;           // points from b toward a (separating direction for a)
    float       depth   = 0.f;   // penetration depth
};

// Results returned by CollisionSystem::run() — valid until the next run().
struct CollisionResults {
    uint32_t                 broadCount  = 0;
    uint32_t                 midCount    = 0;
    uint32_t                 narrowCount = 0;
    std::vector<ContactPoint> contacts;
};
