#pragma once
#include "Component.h"

class ComponentCharacterMotion;
class ComponentAnimation;

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

    ComponentCharacterMotion* m_motion      = nullptr;
    ComponentAnimation*       m_anim        = nullptr;
    bool                      m_initialized = false;
    bool                      m_wasMoving   = false;
};
