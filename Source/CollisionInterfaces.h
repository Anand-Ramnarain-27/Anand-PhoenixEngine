#pragma once
#include "Globals.h"
#include "BoundingVolume.h"
#include <vector>
#include <cstdint>

class GameObject;

struct CollisionBody {
    GameObject* go = nullptr;

    AABB worldAABB;

    BVType bvType = BVType::AABB;

    Vector3 obbCenter;
    Vector3 obbAxes[3];
    float obbHalves[3];

    Vector3 sphereCenter;
    float sphereRadius = 0.f;
};

struct CollisionPair {
    uint32_t a;
    uint32_t b;
};

struct ContactPoint {
    GameObject* a = nullptr;
    GameObject* b = nullptr;
    Vector3 point;
    Vector3 normal;
    float depth = 0.f;
};

struct CollisionResults {
    uint32_t broadCount = 0;
    uint32_t midCount = 0;
    uint32_t narrowCount = 0;
    float broadPhaseMs = 0.f;
    std::vector<ContactPoint> contacts;
};

class IBroadPhase {
public:
    virtual ~IBroadPhase() = default;

    virtual std::vector<CollisionPair> query(
        const std::vector<CollisionBody>& bodies) = 0;

    virtual const char* getName() const = 0;

    virtual void drawDebug(){}
};

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
