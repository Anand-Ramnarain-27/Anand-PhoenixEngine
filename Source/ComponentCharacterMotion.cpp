#include "Globals.h"
#include "ComponentCharacterMotion.h"
#include "ComponentTransform.h"
#include "GameObject.h"
#include <imgui.h>
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <cmath>

using namespace rapidjson;

ComponentCharacterMotion::ComponentCharacterMotion(GameObject* owner) : Component(owner){}

void ComponentCharacterMotion::update(float dt){
    ComponentTransform* t = owner->getTransform();
    if (!t) return;

    if (!m_yawInit){
        const Quaternion& q = t->rotation;
        m_yaw = 2.f * atan2f(q.y, q.w);
        m_yawInit = true;
    }

    m_yaw += m_rotateDir * angularSpeed * dt;

    Vector3 forward = { sinf(m_yaw), 0.f, cosf(m_yaw) };
    t->position += forward * (m_moveDir * linearSpeed * dt);
    t->rotation = Quaternion::CreateFromYawPitchRoll(m_yaw, 0.f, 0.f);
    t->markDirty();

    m_moveDir = 0.f;
    m_rotateDir = 0.f;
}

void ComponentCharacterMotion::onEditor(){
    ImGui::DragFloat("Linear Speed", &linearSpeed, 0.1f, 0.f, 100.f);
    ImGui::DragFloat("Angular Speed", &angularSpeed, 0.01f, 0.f, 20.f);
    ImGui::LabelText("Yaw (rad)", "%.3f", m_yaw);
}

void ComponentCharacterMotion::onSave(std::string& outJson) const{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("linearSpeed", linearSpeed, a);
    doc.AddMember("angularSpeed", angularSpeed, a);
    doc.AddMember("yaw", m_yaw, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentCharacterMotion::onLoad(const std::string& jsonStr){
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("linearSpeed")) linearSpeed = doc["linearSpeed"].GetFloat();
    if (doc.HasMember("angularSpeed")) angularSpeed = doc["angularSpeed"].GetFloat();
    if (doc.HasMember("yaw")){ m_yaw = doc["yaw"].GetFloat(); m_yawInit = true; }
}
