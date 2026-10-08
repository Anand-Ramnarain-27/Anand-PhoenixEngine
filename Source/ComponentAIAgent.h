#pragma once
// Navigation agent: idle / patrol / chase along NavigationSystem paths.

#include "Component.h"
#include "Globals.h"
#include <string>
#include <vector>
#include <cfloat>

/// Idle/Patrol/Chase agent: requests paths from NavigationSystem and moves kinematically along them via
/// SteeringBehaviors.
class ComponentAIAgent final : public Component {
public:
    explicit ComponentAIAgent(GameObject* owner);

    enum class Behavior { Idle, Patrol, Chase };

    float maxSpeed = 3.5f;
    float maxAccel = 12.f;
    float arriveRadius = 1.5f;
    float detectionRange = 6.f;

    std::string targetName = "Player";
    std::vector<Vector3> patrolPoints;

    Behavior getBehavior() const { return m_behavior; }
    const std::vector<Vector3>& getCurrentPath() const { return m_currentPath; }

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::AIAgent; }

private:
    void updateBehavior();
    void requestPathTo(const Vector3& goal);

    Behavior m_behavior = Behavior::Patrol;
    Vector3 m_velocity = Vector3::Zero;

    std::vector<Vector3> m_currentPath;
    int m_pathIndex = 0;
    int m_patrolTargetIndex = 0;
    Vector3 m_lastGoal = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
};
