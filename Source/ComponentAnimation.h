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

    AnimationController& getController() { return m_controller; }
    const std::string& getActiveState() const { return m_activeState; }

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
