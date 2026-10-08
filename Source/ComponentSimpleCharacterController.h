#pragma once
// Keyboard-driven test character: drives ComponentCharacterMotion and animation triggers.

#include "Component.h"

class ComponentCharacterMotion;
class ComponentAnimation;

/// WASD movement through a sibling ComponentCharacterMotion, firing the matching triggers on a sibling
/// ComponentAnimation. A test harness for the animation pipeline, not gameplay code.
class ComponentSimpleCharacterController final : public Component {
public:
    explicit ComponentSimpleCharacterController(GameObject* owner);

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::SimpleCharacterController; }

private:
    void ensureInit();

    ComponentCharacterMotion* m_motion = nullptr;
    ComponentAnimation* m_anim = nullptr;
    bool m_initialized = false;
    bool m_wasMoving = false;
    bool m_wasRunning = false;
    bool m_isDead = false;
    bool m_kWasDown = false;
};
