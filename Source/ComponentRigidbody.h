#pragma once
#include "Component.h"
#include "Globals.h"

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

    float getInvMass() const{
        return (isStatic || mass <= 0.f) ? 0.f : 1.f / mass;
    }

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Rigidbody; }
};
