#pragma once
#include "Component.h"
#include "AnimationController.h"
#include "ResourceCommon.h"
#include "ResourceStateMachine.h"
#include <vector>
#include <string>
#include <memory>

class ResourceAnimation;
class StateMachineGraphEditor;

struct AnimLayer {
    ResourceAnimation* anim = nullptr;
    float currentTimeMs = 0.f;
    float fadeTimeMs = 0.f;
    float transitionTimeMs = 0.f;
    bool loop = false;
    AnimLayer* next = nullptr;
};

class ComponentAnimation final : public Component {
public:
    explicit ComponentAnimation(GameObject* owner);
    ~ComponentAnimation() override;

    void OnPlay(UID uid, bool loop = false);
    void OnStop();

    void OnPlay();

    void SetStateMachine(ResourceStateMachine* sm){ m_stateMachine = sm; }

    void LoadStateMachineFromPath(const std::string& path);

    ResourceStateMachine* getStateMachine(){ return m_stateMachine; }
    const ResourceStateMachine* getStateMachine() const { return m_stateMachine; }
    const HashString& getActiveState() const { return m_activeState; }
    const AnimLayer* getLayerHead() const { return m_layerHead; }
    int getLayerCount() const{
        int n = 0;
        for (const AnimLayer* l = m_layerHead; l; l = l->next) ++n;
        return n;
    }

    void SendTrigger(const HashString& trigger);

    void drawStateMachineSection();

    float mSpeed = 1.f;

    void setAnimationList(const std::vector<UID>& uids);
    const std::vector<UID>& getAnimationUIDs() const { return m_animUIDs; }

    void update(float deltaTime) override;
    void onEditor() override;
    void onDrawGizmos() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Animation; }

    AnimationController& getController(){ return m_controller; }
    const AnimationController& getController() const { return m_controller; }

    bool& drawBones(){ return m_drawBones; }
    bool& drawAxisTriads(){ return m_drawAxisTriads; }

private:
    void pushLayer(UID animUID, float transitionTimeMs, bool loop);
    void freeLayerChain(AnimLayer* head);
    void clearLayers();

    void GetBlendedTransform(const char* name, AnimLayer* layer,
                              Vector3& pos, Quaternion& rot) const;
    void GetBlendedMorphWeights(const char* name, AnimLayer* layer,
                                 float* weights, uint32_t count) const;

    void applyAnimation(GameObject* go);
    void applyBlendedAnimation(GameObject* go);

    AnimationController m_controller;
    std::vector<UID> m_animUIDs;
    std::vector<std::string> m_animNames;

    ResourceStateMachine* m_stateMachine = nullptr;
    std::unique_ptr<ResourceStateMachine> m_ownedStateMachine;
    std::string m_stateMachinePath;
    HashString m_activeState;
    AnimLayer* m_layerHead = nullptr;

    std::unique_ptr<StateMachineGraphEditor> m_graphEditor;

    bool m_drawBones = false;
    bool m_drawAxisTriads = false;
    float m_logTimer = 0.f;

    bool isOwnerMeshVisible() const;
};
