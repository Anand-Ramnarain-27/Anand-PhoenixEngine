#pragma once
#include "Component.h"
#include "AnimationController.h"
#include "StateMachineResource.h"
#include <string>
#include <unordered_map>

class ResourceAnimation;

class ComponentAnimation : public Component {
public:
    explicit ComponentAnimation(GameObject* owner);
    ~ComponentAnimation() override;

    void update(float deltaTime) override;
    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json)  override;
    Type getType() const override { return Type::Animation; }

    void onPlay();
    void onStop();

    void sendTrigger(const std::string& triggerName);

    void setStateMachineResource(StateMachineResource* res);
    StateMachineResource* getStateMachineResource() { return m_smResource; }

    void registerClip(const std::string& clipName, ResourceAnimation* anim);

    const std::unordered_map<std::string, ResourceAnimation*>& getClipMap() const { return m_clipMap; }

    AnimationController& getController() { return m_controller; }
    const std::string& getActiveState() const { return m_activeState; }

    // Returns the owning GameObject (the Component base class stores it as protected)
    GameObject* getOwner() const { return owner; }

    bool m_debugDrawSkeleton = false;

    bool isDebugDrawEnabled() const { return m_debugDrawSkeleton; }
    void setDebugDrawEnabled(bool v) { m_debugDrawSkeleton = v; }

private:
    void applyToHierarchy(GameObject* go);

    ResourceAnimation* resolveClip(const std::string& clipName) const;

    void executeTransition(const SMTransition& tr);

    AnimationController m_controller;
    StateMachineResource* m_smResource = nullptr;
    std::string           m_activeState;
    std::string           m_smFilePath;

    std::unordered_map<std::string, ResourceAnimation*> m_clipMap;
};