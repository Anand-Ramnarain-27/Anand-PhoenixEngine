#pragma once
#include "Globals.h"
#include <vector>

namespace SteeringBehaviors {

    struct SteeringParams {
        float maxSpeed = 3.5f;
        float maxAccel = 12.f;
        float arriveRadius = 1.5f;
        float stopRadius = 0.15f;
    };

    Vector3 Seek(const Vector3& position, const Vector3& target, const SteeringParams& p);
    Vector3 Arrive(const Vector3& position, const Vector3& target, const SteeringParams& p);
    Vector3 Flee(const Vector3& position, const Vector3& target, const SteeringParams& p);

    Vector3 Pursue(const Vector3& position, const Vector3& targetPos, const Vector3& targetVelocity,
                    const SteeringParams& p, float maxPredictionSeconds = 1.f);
    Vector3 Evade(const Vector3& position, const Vector3& targetPos, const Vector3& targetVelocity,
                   const SteeringParams& p, float maxPredictionSeconds = 1.f);

    Vector3 ObstacleAvoidance(const Vector3& position, const std::vector<Vector3>& obstaclePositions,
                               float avoidRadius, const SteeringParams& p);

    // Advances *pathIndex past waypoints already reached (within p.stopRadius).
    Vector3 PathFollow(const Vector3& position, const std::vector<Vector3>& path, int* pathIndex,
                        const SteeringParams& p);

    Vector3 ApplySteering(const Vector3& currentVelocity, const Vector3& desiredVelocity,
                           const SteeringParams& p, float dt);
}
