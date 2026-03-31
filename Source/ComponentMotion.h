#pragma once
#include "Component.h"

class ComponentMotion : public Component {
public:
    explicit ComponentMotion(GameObject* owner);
    void update(float deltaTime) override;
    void onEditor() override;
    void onSave(std::string& out) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Motion; }

    void move(float dir);
    void rotate(float dir);
    void stop();

    float linearSpeed = 3.0f;  
    float angularSpeed = 90.0f;

private:
    float m_linearDir = 0.0f; 
    float m_angularDir = 0.0f;  
    float m_angleY = 0.0f;
};
