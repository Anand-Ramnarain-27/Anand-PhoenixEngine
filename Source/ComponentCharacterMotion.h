#pragma once
// Kinematic tank-style movement: move along the facing, turn about Y.

#include "Component.h"

/// Applies Move() / Rotate() inputs (-1..1) to the transform each update at linearSpeed / angularSpeed.
class ComponentCharacterMotion final : public Component {
public:
    explicit ComponentCharacterMotion(GameObject* owner);

    void Move(float dir){ m_moveDir = dir; }
    void Rotate(float dir){ m_rotateDir = dir; }

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::CharacterMotion; }

    float linearSpeed = 5.f;
    float angularSpeed = 2.f;

private:
    float m_yaw = 0.f;
    float m_moveDir = 0.f;
    float m_rotateDir = 0.f;
    bool m_yawInit = false;
};
