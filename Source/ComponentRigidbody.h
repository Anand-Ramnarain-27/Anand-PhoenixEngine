#pragma once
// Rigid-body state for the collision response solver (mass, velocity, restitution, gravity).

#include "Component.h"
#include "Globals.h"

/// Integrated each update (gravity, damping) and pushed apart by CollisionResponse. Static bodies never move.
class ComponentRigidbody final : public Component {
public:
    explicit ComponentRigidbody(GameObject* owner);

    float mass = 1.f;
    bool isStatic = false;
    float restitution = 0.5f;
    float linearDamping = 0.5f;
    bool useGravity = true;

    Vector3 velocity = {};

    float gravityScale = 1.f;
    static constexpr float kGravityAccel = -9.81f;


    bool useVelocityClamping = true;
    float velocityClampDiameters = 1.f;

    bool isFastMoving = false;

    /// 0 for static or massless bodies, which collision response treats as immovable.
    float getInvMass() const{
        return (isStatic || mass <= 0.f) ? 0.f : 1.f / mass;
    }

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Rigidbody; }
};
