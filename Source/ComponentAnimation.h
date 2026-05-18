#pragma once
#include "Component.h"
#include "AnimController.h"
#include "ResourceCommon.h"
#include <string>
#include <vector>

class ComponentAnimation : public Component {
public:
    explicit ComponentAnimation(GameObject* owner);
    ~ComponentAnimation() override = default;

    void update(float dt) override;
    void onEditor() override;
    void onSave(std::string& outJson)        const override;
    void onLoad(const std::string& json)          override;
    Type getType() const override { return Type::Animation; }

    void play(UID animUID, bool loop = true);
    void stop();
    void setDebugDrawBones(bool v) { m_debugBones = v; }

    const AnimController& getController() const
    {
        return m_controller;
    }

    // List of UIDs available on this model (populated at load)
    std::vector<UID>         m_availableAnims;
    std::vector<std::string> m_availableAnimNames;

private:
    void applyAnimToTree(GameObject* node);
    void debugDrawBones(GameObject* node, const Vector3& parentPos) const;

    AnimController m_controller;
    bool           m_debugBones = false;
};