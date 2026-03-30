#pragma once
#include "Component.h"
#include "AnimController.h"
#include "ResourceCommon.h"
#include <string>

class ResourceAnimation;

class ComponentAnimation : public Component {
public:
    explicit ComponentAnimation(GameObject* owner);
    ~ComponentAnimation() override;

    void update(float dtMs) override;  
    void onEditor() override;
    void onSave(std::string& outJson)  const override;
    void onLoad(const std::string& json)    override;
    Type getType() const override { return Type::Animation; }

    void Play(UID animUID, bool loop = true);
    void Stop();
    bool isPlaying() const { return m_controller.isPlaying(); }

private:
    void updateNode(GameObject* go);

    AnimController m_controller;
    ResourceAnimation* m_animRes = nullptr;
    UID m_animUID = 0;
    bool m_loop = true;
};
