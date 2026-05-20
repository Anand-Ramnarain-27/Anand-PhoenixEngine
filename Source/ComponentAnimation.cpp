#include "Globals.h"
#include "ComponentAnimation.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ModuleResources.h"
#include "ResourceAnimation.h"
#include "Application.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

ComponentAnimation::ComponentAnimation(GameObject* owner) : Component(owner) {}

void ComponentAnimation::setAnimationList(const std::vector<UID>& uids) {
    m_animUIDs.clear();
    m_animNames.clear();
    for (UID uid : uids) {
        m_animUIDs.push_back(uid);
        auto* anim = app->getResources()->RequestAnimation(uid);
        if (anim) {
            std::string name = anim->getAnimName();
            m_animNames.push_back(name.empty() ? ("Anim_" + std::to_string(m_animUIDs.size() - 1)) : name);
            app->getResources()->ReleaseResource(anim);
        } else {
            m_animNames.push_back("Anim_" + std::to_string(m_animUIDs.size() - 1));
        }
    }
}

void ComponentAnimation::OnPlay(UID uid, bool loop) {
    m_controller.Play(uid, loop);
}

void ComponentAnimation::OnStop() {
    m_controller.Stop();
}

void ComponentAnimation::update(float deltaTime) {
    m_controller.Update(deltaTime);
    if (!m_controller.isPlaying()) return;

    Matrix rootWorld = owner->getTransform()->getGlobalMatrix();
    for (auto* child : owner->getChildren())
        applyAnimation(child, rootWorld);
}

void ComponentAnimation::applyAnimation(GameObject* go, const Matrix& parentWorld) {
    auto* t = go->getTransform();

    Vector3 pos = t->position;
    Quaternion rot = t->rotation;
    if (m_controller.GetTransform(go->getName().c_str(), pos, rot)) {
        t->position = pos;
        t->rotation = rot;
        t->markDirty();
    }

    // WorldTransform = LocalTransform * ParentWorldTransform
    Matrix world = t->getLocalMatrix() * parentWorld;

    for (auto* child : go->getChildren())
        applyAnimation(child, world);
}

void ComponentAnimation::onEditor() {
    // Dropdown: pick animation from list
    int currentIdx = -1;
    for (int i = 0; i < (int)m_animUIDs.size(); ++i) {
        if (m_animUIDs[i] == m_controller.Resource) { currentIdx = i; break; }
    }
    const char* preview = (currentIdx >= 0 && currentIdx < (int)m_animNames.size())
                          ? m_animNames[currentIdx].c_str() : "None";

    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##animsel", preview)) {
        for (int i = 0; i < (int)m_animUIDs.size(); ++i) {
            bool selected = (i == currentIdx);
            if (ImGui::Selectable(m_animNames[i].c_str(), selected))
                m_controller.Play(m_animUIDs[i], m_controller.Loop);
            if (selected) ImGui::SetItemDefaultFocus();
        }
        if (m_animUIDs.empty()) { ImGui::TextDisabled("No animations loaded"); }
        ImGui::EndCombo();
    }
    ImGui::Spacing();

    // Play / Stop
    if (m_controller.isPlaying()) {
        if (ImGui::Button("Stop")) m_controller.Stop();
    } else {
        if (ImGui::Button("Play")) {
            UID uid = m_controller.Resource;
            if (uid == 0 && !m_animUIDs.empty()) uid = m_animUIDs[0];
            if (uid != 0) m_controller.Play(uid, m_controller.Loop);
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &m_controller.Loop);

    // Time slider
    float duration = 0.f;
    if (m_controller.Resource != 0) {
        auto* anim = app->getResources()->RequestAnimation(m_controller.Resource);
        if (anim) { duration = anim->getDuration(); app->getResources()->ReleaseResource(anim); }
    }
    if (duration > 0.f) {
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##animtime", &m_controller.CurrentTime, 0.f, duration, "%.2f s");
        ImGui::SameLine(0, 4); ImGui::TextDisabled("/ %.2f", duration);
    }
}

void ComponentAnimation::onSave(std::string& outJson) const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("animUID",    Value(static_cast<uint64_t>(m_controller.Resource)), a);
    doc.AddMember("loop",       Value(m_controller.Loop), a);
    doc.AddMember("playing",    Value(m_controller.isPlaying()), a);
    doc.AddMember("currentTime",Value(m_controller.CurrentTime), a);

    // Save full animation UID list so it survives scene save/load
    Value uids(kArrayType);
    for (UID uid : m_animUIDs) uids.PushBack(Value(static_cast<uint64_t>(uid)), a);
    doc.AddMember("animUIDs", uids, a);

    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentAnimation::onLoad(const std::string& json) {
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()) { LOG("ComponentAnimation: JSON parse error"); return; }

    // Restore animation list
    if (doc.HasMember("animUIDs") && doc["animUIDs"].IsArray()) {
        std::vector<UID> uids;
        for (const auto& v : doc["animUIDs"].GetArray())
            uids.push_back(static_cast<UID>(v.GetUint64()));
        setAnimationList(uids);
    }

    UID uid  = doc.HasMember("animUID") ? static_cast<UID>(doc["animUID"].GetUint64()) : 0;
    bool loop    = doc.HasMember("loop")    ? doc["loop"].GetBool()    : false;
    bool playing = doc.HasMember("playing") ? doc["playing"].GetBool() : false;
    float time   = doc.HasMember("currentTime") ? doc["currentTime"].GetFloat() : 0.f;

    if (uid != 0) {
        m_controller.Play(uid, loop);
        if (!playing) m_controller.Stop();
        m_controller.CurrentTime = time;
    }
}
