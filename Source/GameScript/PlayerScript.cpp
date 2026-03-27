#include "PlayerScript.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
using namespace rapidjson;

// LOG is available because GameScripts links against Engine.lib
#include "Globals.h"

PlayerScript::PlayerScript() = default;

void PlayerScript::onStart(GameObject* owner) {
    m_owner = owner;
    m_timer = 0.0f;
    //LOG("[PlayerScript] onStart — owner: %s", owner ? owner->getName().c_str() : "null");
}

void PlayerScript::onUpdate(float dt) {
    m_timer += dt;
    // TODO: add real player movement / input here
}

void PlayerScript::onDestroy() {
}

void PlayerScript::onEditor() {
}

std::string PlayerScript::onSave() const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("speed", m_speed, a);
    doc.AddMember("timer", m_timer, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    return buf.GetString();
}

void PlayerScript::onLoad(const std::string& json) {
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("speed")) m_speed = doc["speed"].GetFloat();
    if (doc.HasMember("timer")) m_timer = doc["timer"].GetFloat();
}

// ?? Factory ?????????????????????????????????????????????????????????????????
// HotReloadManager calls this to get a new PlayerScript instance.
// extern "C" prevents C++ name-mangling so GetProcAddress finds it by name.
IScript* Create_PlayerScript() { return new PlayerScript(); }
