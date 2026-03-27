#include "EnemyScript.h"
#include <imgui.h>
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include "Globals.h"
using namespace rapidjson;

EnemyScript::EnemyScript() = default;

void EnemyScript::onStart(GameObject* owner) {
    m_owner = owner;
    m_isAggro = false;
}

void EnemyScript::onUpdate(float dt) {
    (void)dt;
}

void EnemyScript::onDestroy() {

}

void EnemyScript::onEditor() {

}

std::string EnemyScript::onSave() const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("range", m_detectionRange, a);
    doc.AddMember("aggro", m_isAggro, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    return buf.GetString();
}

void EnemyScript::onLoad(const std::string& json) {
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("range")) m_detectionRange = doc["range"].GetFloat();
    if (doc.HasMember("aggro")) m_isAggro = doc["aggro"].GetBool();
}

IScript* Create_EnemyScript() { return new EnemyScript(); }
