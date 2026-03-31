#include "Globals.h"
#include "ComponentMotion.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include <imgui.h>
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <cmath>

static constexpr float kDeg2Rad = 3.14159265f / 180.0f;

ComponentMotion::ComponentMotion(GameObject* owner) : Component(owner) {}

void ComponentMotion::move(float dir) { m_linearDir = dir; }
void ComponentMotion::rotate(float dir) { m_angularDir = dir; }
void ComponentMotion::stop() { m_linearDir = 0.0f; m_angularDir = 0.0f; }

void ComponentMotion::update(float deltaTime) {
    auto* t = owner ? owner->getTransform() : nullptr;
    if (!t) return;

    m_angleY += m_angularDir * angularSpeed * deltaTime;

    if (m_linearDir != 0.0f) {
        float rad = m_angleY * kDeg2Rad;
        Vector3 forward = {
            std::sinf(rad),
            0.0f,
            std::cosf(rad)
        };
        t->position += forward * (m_linearDir * linearSpeed * deltaTime);
    }

    t->rotation = Quaternion::CreateFromAxisAngle(
        Vector3::UnitY, m_angleY * kDeg2Rad);
    t->markDirty();
}

void ComponentMotion::onEditor() {
    ImGui::Text("Motion Component");
    ImGui::Separator();
    ImGui::DragFloat("Linear speed (m/s)##mot", &linearSpeed, 0.1f, 0.0f, 20.0f);
    ImGui::DragFloat("Angular speed (deg/s)##mot", &angularSpeed, 1.0f, 0.0f, 360.0f);
    ImGui::Text("Angle Y: %.1f deg", m_angleY);
    auto* t = owner ? owner->getTransform() : nullptr;
    if (t) ImGui::Text("Pos: %.2f, %.2f, %.2f",
        t->position.x, t->position.y, t->position.z);
}

void ComponentMotion::onSave(std::string& out) const {
    using namespace rapidjson;
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("linearSpeed", linearSpeed, a);
    doc.AddMember("angularSpeed", angularSpeed, a);
    StringBuffer sb; Writer<StringBuffer> w(sb); doc.Accept(w);
    out = sb.GetString();
}

void ComponentMotion::onLoad(const std::string& json) {
    using namespace rapidjson;
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("linearSpeed"))  linearSpeed = doc["linearSpeed"].GetFloat();
    if (doc.HasMember("angularSpeed")) angularSpeed = doc["angularSpeed"].GetFloat();
}
