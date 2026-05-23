#pragma once
#include "Component.h"
#include "AnimationController.h"
#include "ResourceCommon.h"
#include <vector>
#include <string>

class ComponentAnimation final : public Component {
public:
    explicit ComponentAnimation(GameObject* owner);
    ~ComponentAnimation() override = default;

    void OnPlay(UID uid, bool loop = false);
    void OnStop();

    // Populate the animation list shown in the inspector dropdown.
    // Names are fetched from ResourceAnimation::getAnimName() and cached here.
    void setAnimationList(const std::vector<UID>& uids);
    const std::vector<UID>& getAnimationUIDs() const { return m_animUIDs; }

    void update(float deltaTime) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Animation; }

    AnimationController& getController() { return m_controller; }
    const AnimationController& getController() const { return m_controller; }

private:
    void applyAnimation(GameObject* go, const Matrix& parentWorld);

    AnimationController      m_controller;
    std::vector<UID>         m_animUIDs;
    std::vector<std::string> m_animNames; // parallel to m_animUIDs
};
