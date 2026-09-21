#pragma once
#include "Globals.h"
#include <vector>

// Pure steering-behavior math (Reynolds-style). No engine/component dependency -
// callers pass current state in and get a desired velocity out.
namespace SteeringBehaviors {

    struct SteeringParams {
        float maxSpeed = 3.5f;
        float maxAccel = 12.f;
        float arriveRadius = 1.5f;   // Arrive: distance at which slowdown begins
        float stopRadius = 0.15f;    // Arrive: distance considered "reached"
    };

    // Seek: desired velocity straight toward target at maxSpeed.
    Vector3 Seek(const Vector3& position, const Vector3& target, const SteeringParams& p);

    // Arrive: like Seek, but decelerates within arriveRadius so it settles at target.
    Vector3 Arrive(const Vector3& position, const Vector3& target, const SteeringParams& p);

    // Flee: desired velocity straight away from target at maxSpeed.
    Vector3 Flee(const Vector3& position, const Vector3& target, const SteeringParams& p);

    // Pursue: Seek toward a predicted future position of a moving target.
    Vector3 Pursue(const Vector3& position, const Vector3& targetPos, const Vector3& targetVelocity,
                    const SteeringParams& p, float maxPredictionSeconds = 1.f);

    // Evade: Flee from a predicted future position of a moving target.
    Vector3 Evade(const Vector3& position, const Vector3& targetPos, const Vector3& targetVelocity,
                   const SteeringParams& p, float maxPredictionSeconds = 1.f);

    // ObstacleAvoidance: pushes away from nearby obstacle points within avoidRadius,
    // weighted by proximity. Returns a steering force to blend with another behavior.
    Vector3 ObstacleAvoidance(const Vector3& position, const std::vector<Vector3>& obstaclePositions,
                               float avoidRadius, const SteeringParams& p);

    // PathFollow: seeks toward path[*pathIndex]; advances *pathIndex when within
    // p.stopRadius of the current waypoint. Returns Vector3::Zero once the path is complete.
    Vector3 PathFollow(const Vector3& position, const std::vector<Vector3>& path, int* pathIndex,
                        const SteeringParams& p);

    // Steers current velocity toward desiredVelocity, clamped by maxAccel*dt, then by maxSpeed.
    Vector3 ApplySteering(const Vector3& currentVelocity, const Vector3& desiredVelocity,
                           const SteeringParams& p, float dt);
}
