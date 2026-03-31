#pragma once
#include "Component.h"

class ComponentAnimation;
class ComponentMotion;

class SimpleCharacterController : public Component {
public:
    explicit SimpleCharacterController(GameObject* owner);
    void update(float deltaTime) override;
    void onEditor() override;
    Type getType() const override { return Type::CharacterController; }

    std::string triggerMove = "move"; 
    std::string triggerStop = "stop";  

private:
    bool m_wasMoving = false; 
};
