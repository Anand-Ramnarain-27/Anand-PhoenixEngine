#pragma once
#include "Component.h"
#include "Globals.h"
#include <string>
#include <vector>
#include <cfloat>

// Native AI navigation/movement component (engine-level, not a GameScript) -
// perception + a hardcoded Idle/Patrol/Chase behavior drive path requests
// against the active NavigationSystem, and SteeringBehaviors moves the agent
// kinematically along the returned path. Phase 1: see plan "AI Navigation &
// Movement" for what's deferred (behavior-tree/FSM resource, Recast backend,
// Rigidbody-integrated movement, line-of-sight perception).
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

    Behavior getBehavior() const { return behavior; }
    const std::vector<Vector3>& getCurrentPath() const { return currentPath; }

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::AIAgent; }

private:
    void updateBehavior();
    void requestPathTo(const Vector3& goal);

    Behavior behavior = Behavior::Patrol;
    Vector3 velocity = Vector3::Zero;

    std::vector<Vector3> currentPath;
    int pathIndex = 0;
    int patrolTargetIndex = 0;
    Vector3 lastGoal = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
};
