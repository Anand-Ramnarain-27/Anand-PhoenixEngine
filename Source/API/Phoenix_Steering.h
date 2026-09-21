#pragma once
#include "API/Phoenix_Types.h"
#include "SteeringBehaviors.h"
#include <vector>

namespace Phoenix {

// Thin flat-argument wrapper around SteeringBehaviors:: so scripts don't need
// to know about SteeringBehaviors::SteeringParams - just pass maxSpeed/maxAccel
// directly. Header-only (pure math, no engine singleton), like Phoenix_GameObject.h.
struct Steering {
    static Vec3 Seek(Vec3 position, Vec3 target, float maxSpeed){
        SteeringBehaviors::SteeringParams p; p.maxSpeed = maxSpeed;
        return SteeringBehaviors::Seek(position, target, p);
    }

    static Vec3 Arrive(Vec3 position, Vec3 target, float maxSpeed, float arriveRadius){
        SteeringBehaviors::SteeringParams p; p.maxSpeed = maxSpeed; p.arriveRadius = arriveRadius;
        return SteeringBehaviors::Arrive(position, target, p);
    }

    static Vec3 Flee(Vec3 position, Vec3 target, float maxSpeed){
        SteeringBehaviors::SteeringParams p; p.maxSpeed = maxSpeed;
        return SteeringBehaviors::Flee(position, target, p);
    }

    static Vec3 Pursue(Vec3 position, Vec3 targetPos, Vec3 targetVelocity, float maxSpeed,
                       float maxPredictionSeconds = 1.f){
        SteeringBehaviors::SteeringParams p; p.maxSpeed = maxSpeed;
        return SteeringBehaviors::Pursue(position, targetPos, targetVelocity, p, maxPredictionSeconds);
    }

    static Vec3 Evade(Vec3 position, Vec3 targetPos, Vec3 targetVelocity, float maxSpeed,
                      float maxPredictionSeconds = 1.f){
        SteeringBehaviors::SteeringParams p; p.maxSpeed = maxSpeed;
        return SteeringBehaviors::Evade(position, targetPos, targetVelocity, p, maxPredictionSeconds);
    }

    static Vec3 ObstacleAvoidance(Vec3 position, const std::vector<Vec3>& obstaclePositions,
                                  float avoidRadius, float maxSpeed){
        SteeringBehaviors::SteeringParams p; p.maxSpeed = maxSpeed;
        return SteeringBehaviors::ObstacleAvoidance(position, obstaclePositions, avoidRadius, p);
    }

    static Vec3 ApplySteering(Vec3 currentVelocity, Vec3 desiredVelocity, float maxSpeed, float maxAccel, float dt){
        SteeringBehaviors::SteeringParams p; p.maxSpeed = maxSpeed; p.maxAccel = maxAccel;
        return SteeringBehaviors::ApplySteering(currentVelocity, desiredVelocity, p, dt);
    }
};

} // namespace Phoenix
