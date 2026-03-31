#include "Globals.h"
#include "ComponentAnimation.h"
#include "ResourceAnimation.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include <imgui.h>
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

ComponentAnimation::ComponentAnimation(GameObject* owner) : Component(owner) {}

void ComponentAnimation::play(ResourceAnimation* anim, bool loop) {
    m_currentAnim = anim;
    m_controller.play(anim, loop);
}

void ComponentAnimation::stop() {
    m_controller.stop();
}

void ComponentAnimation::update(float deltaTime) {
    m_controller.update(deltaTime);
    if (owner) applyToHierarchy(owner);
}

void ComponentAnimation::applyToHierarchy(GameObject* go) {
    if (!go) return;
    Vector3    pos;
    Quaternion rot;

    if (m_controller.getTransform(go->getName(), pos, rot)) {
        ComponentTransform* t = go->getTransform();
        if (t) {
            t->position = pos;
            t->rotation = rot;
            t->markDirty(); 
        }
    }
    for (auto* child : go->getChildren())
        applyToHierarchy(child);
}

void ComponentAnimation::onEditor() {
    ImGui::Text("Animation Component");
    ImGui::Text("Time: %.3f s", m_controller.getCurrentTime());
    float spd = m_controller.getSpeed();
    if (ImGui::SliderFloat("Speed", &spd, 0.0f, 4.0f)) m_controller.setSpeed(spd);
    if (m_controller.isPlaying()) { if (ImGui::Button("Stop"))  stop(); }
    else { if (ImGui::Button("Play"))  play(m_currentAnim); }
}

void ComponentAnimation::onSave(std::string& outJson) const {
    using namespace rapidjson;
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("animPath", Value(m_animPath.c_str(), a), a);
    doc.AddMember("loop", m_controller.isPlaying(), a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentAnimation::onLoad(const std::string& json) {
    using namespace rapidjson;
    Document doc; doc.Parse(json.c_str());
    if (doc.HasMember("animPath")) m_animPath = doc["animPath"].GetString();
}
