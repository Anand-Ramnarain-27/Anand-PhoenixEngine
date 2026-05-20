#include "Globals.h"
#include "ComponentAnimation.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

ComponentAnimation::ComponentAnimation(GameObject* owner) : Component(owner) {}

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

void ComponentAnimation::onSave(std::string& outJson) const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("animUID",    Value(static_cast<uint64_t>(m_controller.Resource)), a);
    doc.AddMember("loop",       Value(m_controller.Loop), a);
    doc.AddMember("playing",    Value(m_controller.isPlaying()), a);
    doc.AddMember("currentTime",Value(m_controller.CurrentTime), a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentAnimation::onLoad(const std::string& json) {
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()) { LOG("ComponentAnimation: JSON parse error"); return; }

    UID uid  = doc.HasMember("animUID") ? static_cast<UID>(doc["animUID"].GetUint64()) : 0;
    bool loop    = doc.HasMember("loop")    ? doc["loop"].GetBool()    : false;
    bool playing = doc.HasMember("playing") ? doc["playing"].GetBool() : false;
    float time   = doc.HasMember("currentTime") ? doc["currentTime"].GetFloat() : 0.f;

    if (uid != 0 && playing) {
        m_controller.Play(uid, loop);
        m_controller.CurrentTime = time;
    }
}
