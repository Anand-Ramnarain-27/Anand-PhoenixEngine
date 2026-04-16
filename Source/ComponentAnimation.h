#pragma once
#include "Component.h"
#include "AnimationController.h"
#include <string>

class ResourceAnimation;

class ComponentAnimation : public Component {
public:
    explicit ComponentAnimation(GameObject* owner);

    void update(float deltaTime) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Animation; }

    void play(ResourceAnimation* anim, bool loop = true);
    void stop();

    AnimationController& getController() { return m_controller; }

private:
    void applyToHierarchy(GameObject* go);

    AnimationController m_controller;
    ResourceAnimation* m_currentAnim = nullptr;
    std::string         m_animPath;
};
