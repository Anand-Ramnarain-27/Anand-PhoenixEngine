#include "Globals.h"
#include "ComponentAnimation.h"
#include "ResourceAnimation.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "Application.h"
#include "ModuleResources.h"
#include <imgui.h>
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/prettywriter.h"
#include "3rdParty/rapidjson/stringbuffer.h"

ComponentAnimation::ComponentAnimation(GameObject* owner) : Component(owner) {}
ComponentAnimation::~ComponentAnimation() = default;
 
void ComponentAnimation::onPlay() {
    if (!m_smResource || m_smResource->defaultState.empty()) return;
    m_activeState = m_smResource->defaultState;
    const SMState* st = m_smResource->findState(m_activeState);
    if (!st) { LOG("ComponentAnimation: default state not found"); return; }
    const SMClip* cl = m_smResource->findClip(st->clipName);
    if (!cl) { LOG("ComponentAnimation: clip '%s' not found", st->clipName.c_str()); return; }
    ResourceAnimation* anim = resolveClip(cl->name); 
    m_controller.play(anim, cl->loop, 0.0f, 1.0f);
}
 
void ComponentAnimation::onStop() {
    m_controller.stop();
}
 
void ComponentAnimation::update(float deltaTime) {
    float dtMs = deltaTime * 1000.0f;
    m_controller.update(dtMs);
    if (owner) applyToHierarchy(owner);
}
 
void ComponentAnimation::applyToHierarchy(GameObject* go) {
    if (!go) return;
    Vector3    pos = {};
    Quaternion rot = Quaternion::Identity;
    if (m_controller.getTransform(go->getName(), pos, rot)) {
        if (auto* t = go->getTransform()) {
            t->position = pos;
            t->rotation = rot;
            t->markDirty();
        }
    }
    for (auto* child : go->getChildren())
        applyToHierarchy(child);
}
 
void ComponentAnimation::sendTrigger(const std::string& triggerName) {
    if (!m_smResource) return;
    const SMTransition* tr =
        m_smResource->findTransition(m_activeState, triggerName);
    if (!tr) {
        LOG("ComponentAnimation: no transition from '%s' via '%s'",
            m_activeState.c_str(), triggerName.c_str());
        return;
    }
    executeTransition(*tr);
}
 
void ComponentAnimation::executeTransition(const SMTransition& tr) {
    m_activeState = tr.target;
    const SMState* st = m_smResource->findState(tr.target);
    if (!st) { LOG("ComponentAnimation: target state '%s' not found", tr.target.c_str()); return; }
    const SMClip* cl = m_smResource->findClip(st->clipName);
    if (!cl) { LOG("ComponentAnimation: clip '%s' not found", st->clipName.c_str()); return; }
    ResourceAnimation* anim = resolveClip(cl->name);
 
    m_controller.play(anim, cl->loop, tr.interpolationMs, 1.0f);
}

void ComponentAnimation::setStateMachineResource(StateMachineResource* res) {
    m_smResource = res;
    m_activeState.clear();
}

void ComponentAnimation::registerClip(
    const std::string& clipName, ResourceAnimation* anim)
{
    m_clipMap[clipName] = anim;
}

ResourceAnimation* ComponentAnimation::resolveClip(
    const std::string& clipName) const
{
    auto it = m_clipMap.find(clipName);
    if (it != m_clipMap.end()) return it->second;
    LOG("ComponentAnimation: clip '%s' not registered in clip map",
        clipName.c_str());
    return nullptr;
}
 
void ComponentAnimation::onEditor() {
    ImGui::Text("State Machine Component");
    ImGui::Separator();
    ImGui::Text("Active state: %s",
        m_activeState.empty() ? "(none)" : m_activeState.c_str());
    ImGui::Text("Playing:      %s",
        m_controller.isPlaying() ? "yes" : "no");
    ImGui::Text("Time:         %.1f ms",
        m_controller.getCurrentTimeMs());
    float spd = m_controller.getSpeed();
    if (ImGui::SliderFloat("Speed##anim", &spd, 0.0f, 4.0f))
        m_controller.setSpeed(spd);
    if (ImGui::Button("Play##anim"))  onPlay();
    ImGui::SameLine();
    if (ImGui::Button("Stop##anim"))  onStop(); 
    if (m_smResource && !m_activeState.empty()) {
        ImGui::Separator();
        ImGui::Text("Triggers:");
        for (const auto& tr : m_smResource->transitions) {
            if (tr.source != m_activeState) continue;
            std::string btn = tr.triggerName + "##tr";
            if (ImGui::Button(btn.c_str()))
                sendTrigger(tr.triggerName);
        }
    }

    ImGui::Separator();
    ImGui::Checkbox("Debug Skeleton##anim", &m_debugDrawSkeleton);
}
 
void ComponentAnimation::onSave(std::string& outJson) const {
    using namespace rapidjson;
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("smFilePath", Value(m_smFilePath.c_str(), a), a);
    doc.AddMember("activeState", Value(m_activeState.c_str(), a), a);
    doc.AddMember("debugSkeleton", m_debugDrawSkeleton, a);
    StringBuffer sb; PrettyWriter<StringBuffer> w(sb); doc.Accept(w);
    outJson = sb.GetString();
}

void ComponentAnimation::onLoad(const std::string& json) {
    using namespace rapidjson;
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("smFilePath"))
        m_smFilePath = doc["smFilePath"].GetString();
    if (doc.HasMember("activeState"))
        m_activeState = doc["activeState"].GetString();
    if (doc.HasMember("debugSkeleton"))
        m_debugDrawSkeleton = doc["debugSkeleton"].GetBool();
    // TODO: resolve m_smFilePath -> load StateMachineResource
    // TODO: for each clip in resource -> RequestAnimation -> registerClip
}
