#pragma once
#include "Component.h"
#include "Globals.h"

// Physics body attached to a GameObject.
//
// Dynamic objects have mass > 0 and isStatic = false.  Each frame their
// velocity is integrated into the transform by update().
//
// Static objects (isStatic = true OR mass <= 0) never move from collision
// response but still participate as immovable obstacles.
class ComponentRigidbody final : public Component {
public:
    explicit ComponentRigidbody(GameObject* owner);

    // ---- Inspector-visible properties ----
    float   mass          = 1.f;   // kg. 0 or isStatic = immovable.
    bool    isStatic      = false;
    float   restitution   = 0.5f;  // bounciness: 0 = inelastic, 1 = elastic
    float   linearDamping = 0.5f;  // velocity drag (fraction lost per second)
    bool    useGravity    = true;

    // Current world-space velocity (units/second).  Set an initial value in
    // the Inspector before pressing Play to give the object a launch velocity.
    Vector3 velocity = {};

    // Gravity acceleration (world-space). Negative Y = downward.
    float   gravityScale = 1.f;
    static constexpr float kGravityAccel = -9.81f; // m/s²

    // ---- Helpers used by CollisionResponse ----
    float getInvMass() const {
        return (isStatic || mass <= 0.f) ? 0.f : 1.f / mass;
    }

    // ---- Component interface ----
    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Rigidbody; }
};
