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

ComponentCharacterMotion::ComponentCharacterMotion(GameObject* owner) : Component(owner) {}

void ComponentCharacterMotion::update(float dt) {
    ComponentTransform* t = owner->getTransform();
    if (!t) return;

    if (!m_yawInit) {
        const Quaternion& q = t->rotation;
        mYaw = 2.f * atan2f(q.y, q.w);
        m_yawInit = true;
    }

    mYaw += mRotateDir * mAngularSpeed * dt;

    Vector3 forward = { sinf(mYaw), 0.f, cosf(mYaw) };
    t->position += forward * (mMoveDir * mLinearSpeed * dt);
    t->rotation  = Quaternion::CreateFromYawPitchRoll(mYaw, 0.f, 0.f);
    t->markDirty();

    mMoveDir   = 0.f;
    mRotateDir = 0.f;
}

void ComponentCharacterMotion::onEditor() {
    ImGui::DragFloat("Linear Speed",  &mLinearSpeed,  0.1f,  0.f, 100.f);
    ImGui::DragFloat("Angular Speed", &mAngularSpeed, 0.01f, 0.f,  20.f);
    ImGui::LabelText("Yaw (rad)", "%.3f", mYaw);
}

void ComponentCharacterMotion::onSave(std::string& outJson) const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("linearSpeed",  mLinearSpeed,  a);
    doc.AddMember("angularSpeed", mAngularSpeed, a);
    doc.AddMember("yaw",          mYaw,          a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentCharacterMotion::onLoad(const std::string& jsonStr) {
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("linearSpeed"))  mLinearSpeed  = doc["linearSpeed"].GetFloat();
    if (doc.HasMember("angularSpeed")) mAngularSpeed = doc["angularSpeed"].GetFloat();
    if (doc.HasMember("yaw"))        { mYaw = doc["yaw"].GetFloat(); m_yawInit = true; }
}
