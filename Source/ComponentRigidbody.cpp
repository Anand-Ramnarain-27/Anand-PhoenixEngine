#include "Globals.h"
#include "ComponentRigidbody.h"
#include "ComponentTransform.h"
#include "GameObject.h"
#include <imgui.h>
#include <algorithm>
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

ComponentRigidbody::ComponentRigidbody(GameObject* owner) : Component(owner) {}

void ComponentRigidbody::update(float dt) {
    if (isStatic || mass <= 0.f) return;

    // Apply gravity
    if (useGravity) velocity.y += kGravityAccel * gravityScale * dt;

    // Exponential velocity damping: preserves units regardless of frame rate
    float dampFactor = std::max(0.f, 1.f - linearDamping * dt);
    velocity *= dampFactor;

    // Zero out very small velocities to prevent endless micro-jitter
    if (velocity.LengthSquared() < 1e-6f) velocity = Vector3::Zero;

    // Integrate position
    ComponentTransform* t = owner->getTransform();
    if (t) {
        t->position += velocity * dt;
        t->markDirty();
    }
}

void ComponentRigidbody::onEditor() {
    ImGui::SeparatorText("Body");
    ImGui::Checkbox("Is Static", &isStatic);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Static objects never move from collisions.");

    if (!isStatic) {
        ImGui::DragFloat("Mass (kg)", &mass, 0.1f, 0.001f, 10000.f, "%.3f");

        ImGui::SeparatorText("Motion");
        ImGui::Checkbox("Use Gravity", &useGravity);
        ImGui::DragFloat("Gravity Scale", &gravityScale, 0.05f, -10.f, 10.f, "%.2f");
        ImGui::DragFloat("Linear Damping", &linearDamping, 0.01f, 0.f, 1.f, "%.3f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Fraction of velocity removed per second (0 = no drag).");

        ImGui::SeparatorText("Initial Velocity");
        float v[3] = { velocity.x, velocity.y, velocity.z };
        if (ImGui::DragFloat3("Velocity", v, 0.1f, -500.f, 500.f, "%.2f"))
            velocity = { v[0], v[1], v[2] };

        ImGui::SeparatorText("Collision Response");
    }
    ImGui::SliderFloat("Restitution", &restitution, 0.f, 1.f, "%.2f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("0 = no bounce (inelastic)   1 = full bounce (elastic)");
}

void ComponentRigidbody::onSave(std::string& outJson) const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("mass",          mass,          a);
    doc.AddMember("isStatic",      isStatic,      a);
    doc.AddMember("restitution",   restitution,   a);
    doc.AddMember("linearDamping", linearDamping, a);
    doc.AddMember("useGravity",    useGravity,    a);
    doc.AddMember("gravityScale",  gravityScale,  a);
    Value vel(kArrayType);
    vel.PushBack(velocity.x, a).PushBack(velocity.y, a).PushBack(velocity.z, a);
    doc.AddMember("velocity", vel, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentRigidbody::onLoad(const std::string& jsonStr) {
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("mass"))          mass          = doc["mass"].GetFloat();
    if (doc.HasMember("isStatic"))      isStatic      = doc["isStatic"].GetBool();
    if (doc.HasMember("restitution"))   restitution   = doc["restitution"].GetFloat();
    if (doc.HasMember("linearDamping")) linearDamping = doc["linearDamping"].GetFloat();
    if (doc.HasMember("useGravity"))    useGravity    = doc["useGravity"].GetBool();
    if (doc.HasMember("gravityScale"))  gravityScale  = doc["gravityScale"].GetFloat();
    if (doc.HasMember("velocity")) {
        const auto& v = doc["velocity"];
        velocity = { v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat() };
    }
}
