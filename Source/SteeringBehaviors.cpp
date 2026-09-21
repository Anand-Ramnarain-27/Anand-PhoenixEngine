#include "Globals.h"
#include "SteeringBehaviors.h"
#include <algorithm>

namespace SteeringBehaviors {

    Vector3 Seek(const Vector3& position, const Vector3& target, const SteeringParams& p){
        Vector3 toTarget = target - position;
        if (toTarget.LengthSquared() < 1e-6f) return Vector3::Zero;
        toTarget.Normalize();
        return toTarget * p.maxSpeed;
    }

    Vector3 Arrive(const Vector3& position, const Vector3& target, const SteeringParams& p){
        Vector3 toTarget = target - position;
        float dist = toTarget.Length();
        if (dist < p.stopRadius) return Vector3::Zero;

        float speed = p.maxSpeed;
        if (dist < p.arriveRadius)
            speed = p.maxSpeed * (dist / p.arriveRadius);

        toTarget.Normalize();
        return toTarget * speed;
    }

    Vector3 Flee(const Vector3& position, const Vector3& target, const SteeringParams& p){
        Vector3 away = position - target;
        if (away.LengthSquared() < 1e-6f) return Vector3::Zero;
        away.Normalize();
        return away * p.maxSpeed;
    }

    Vector3 Pursue(const Vector3& position, const Vector3& targetPos, const Vector3& targetVelocity,
                    const SteeringParams& p, float maxPredictionSeconds){
        float dist = (targetPos - position).Length();
        float t = (p.maxSpeed > 1e-4f) ? std::min(dist / p.maxSpeed, maxPredictionSeconds) : maxPredictionSeconds;
        Vector3 predicted = targetPos + targetVelocity * t;
        return Seek(position, predicted, p);
    }

    Vector3 Evade(const Vector3& position, const Vector3& targetPos, const Vector3& targetVelocity,
                   const SteeringParams& p, float maxPredictionSeconds){
        float dist = (targetPos - position).Length();
        float t = (p.maxSpeed > 1e-4f) ? std::min(dist / p.maxSpeed, maxPredictionSeconds) : maxPredictionSeconds;
        Vector3 predicted = targetPos + targetVelocity * t;
        return Flee(position, predicted, p);
    }

    Vector3 ObstacleAvoidance(const Vector3& position, const std::vector<Vector3>& obstaclePositions,
                               float avoidRadius, const SteeringParams& p){
        if (avoidRadius <= 1e-4f) return Vector3::Zero;

        Vector3 force = Vector3::Zero;
        for (const Vector3& obstacle : obstaclePositions){
            Vector3 away = position - obstacle;
            float dist = away.Length();
            if (dist < 1e-4f || dist >= avoidRadius) continue;

            away.Normalize();
            float weight = (avoidRadius - dist) / avoidRadius;
            force += away * weight;
        }

        if (force.LengthSquared() < 1e-6f) return Vector3::Zero;
        force.Normalize();
        return force * p.maxSpeed;
    }

    Vector3 PathFollow(const Vector3& position, const std::vector<Vector3>& path, int* pathIndex,
                        const SteeringParams& p){
        if (!pathIndex || path.empty()) return Vector3::Zero;
        if (*pathIndex < 0) *pathIndex = 0;

        while (*pathIndex < (int)path.size() &&
               (path[*pathIndex] - position).Length() < p.stopRadius){
            ++(*pathIndex);
        }

        if (*pathIndex >= (int)path.size()) return Vector3::Zero;

        return Arrive(position, path[*pathIndex], p);
    }

    Vector3 ApplySteering(const Vector3& currentVelocity, const Vector3& desiredVelocity,
                           const SteeringParams& p, float dt){
        Vector3 steer = desiredVelocity - currentVelocity;
        float maxDelta = p.maxAccel * dt;
        if (steer.Length() > maxDelta && steer.LengthSquared() > 1e-8f){
            steer.Normalize();
            steer *= maxDelta;
        }

        Vector3 newVelocity = currentVelocity + steer;
        float speed = newVelocity.Length();
        if (speed > p.maxSpeed && speed > 1e-6f)
            newVelocity *= p.maxSpeed / speed;

        return newVelocity;
    }
}
