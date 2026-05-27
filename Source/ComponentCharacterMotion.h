#pragma once
#include "Component.h"

class ComponentCharacterMotion final : public Component {
public:
    explicit ComponentCharacterMotion(GameObject* owner);

    // Called each frame by the controller (or any other system).
    // dir: -1 = backward, 0 = stopped, +1 = forward.
    void Move(float dir)   { mMoveDir   = dir; }
    // dir: -1 = turn left, 0 = stopped, +1 = turn right.
    void Rotate(float dir) { mRotateDir = dir; }

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::CharacterMotion; }

    // Inspector-editable constants.
    float mLinearSpeed  = 5.f;   // units per second
    float mAngularSpeed = 2.f;   // radians per second

private:
    float mYaw       = 0.f;   // current Y-axis rotation in radians
    float mMoveDir   = 0.f;   // written each frame by Move()
    float mRotateDir = 0.f;   // written each frame by Rotate()
    bool  m_yawInit  = false; // true once mYaw has been seeded from the transform
};
