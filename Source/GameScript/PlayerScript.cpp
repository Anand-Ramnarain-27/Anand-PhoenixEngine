#include "PlayerScript.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
using namespace rapidjson;

#include "Globals.h"

PlayerScript::PlayerScript() = default;

void PlayerScript::Start(GameObject* owner){
    m_owner = owner;
    m_timer = 0.0f;
}

void PlayerScript::Update(float dt){
    m_timer += dt;
}

void PlayerScript::Destroy(){
}

void PlayerScript::Editor(){
}

std::string PlayerScript::Save() const{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("speed", m_speed, a);
    doc.AddMember("timer", m_timer, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    return buf.GetString();
}

void PlayerScript::Load(const std::string& json){
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("speed")) m_speed = doc["speed"].GetFloat();
    if (doc.HasMember("timer")) m_timer = doc["timer"].GetFloat();
}

IScript* Create_PlayerScript(){ return new PlayerScript(); }
