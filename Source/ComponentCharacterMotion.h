#pragma once
#include "Component.h"

class ComponentCharacterMotion final : public Component {
public:
    explicit ComponentCharacterMotion(GameObject* owner);

    void Move(float dir){ mMoveDir = dir; }
    void Rotate(float dir){ mRotateDir = dir; }

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::CharacterMotion; }

    float mLinearSpeed = 5.f;
    float mAngularSpeed = 2.f;

private:
    float mYaw = 0.f;
    float mMoveDir = 0.f;
    float mRotateDir = 0.f;
    bool m_yawInit = false;
};
