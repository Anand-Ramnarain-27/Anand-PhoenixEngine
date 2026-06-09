#include "Globals.h"
#include "ComponentRigidbody.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "GameObject.h"
#include "Application.h"
#include "ModuleEditor.h"
#include "SceneManager.h"
#include <imgui.h>
#include <algorithm>
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

ComponentRigidbody::ComponentRigidbody(GameObject* owner) : Component(owner) {}

void ComponentRigidbody::update(float dt){
    if (isStatic || mass <= 0.f) return;

    // Apply gravity — read scene-global Y acceleration; fall back to kGravityAccel if no scene loaded.
    float grav = kGravityAccel;
    if (app && app->getEditor() && app->getEditor()->getSceneManager())
        grav = app->getEditor()->getSceneManager()->getSettings().gravityY;
    if (useGravity) velocity.y += grav * gravityScale * dt;

    // Exponential velocity damping: preserves units regardless of frame rate
    float dampFactor = std::max(0.f, 1.f - linearDamping * dt);
    velocity *= dampFactor;

    // Zero out very small velocities to prevent endless micro-jitter
    if (velocity.LengthSquared() < 1e-6f) velocity = Vector3::Zero;

    // ---- Velocity clamping (tunneling prevention) ----
    // Cap speed so the object cannot travel more than velocityClampDiameters
    // times its own smallest world-space dimension in a single frame.  This
    // guarantees the narrow phase always has a chance to detect the contact.
    if (useVelocityClamping && dt > 1e-7f) {
        float diameter = 1.f; // fallback for objects without a mesh
        auto* cm = owner->getComponent<ComponentMesh>();
        if (cm && cm->hasAABB()) {
            Vector3 mn, mx;
            cm->getWorldAABB(mn, mx);
            Vector3 sz = mx - mn;
            // Smallest axis = tightest constraint, safest against tunneling.
            diameter = std::min(sz.x, std::min(sz.y, sz.z));
            if (diameter < 1e-3f) diameter = 1e-3f;
        }
        float maxSpeed = velocityClampDiameters * diameter / dt;
        float speed = velocity.Length();
        if (speed > maxSpeed && speed > 0.f)
            velocity *= maxSpeed / speed;
    }

    // Integrate position
    ComponentTransform* t = owner->getTransform();
    if (t) {
        t->position += velocity * dt;
        t->markDirty();
    }
}

void ComponentRigidbody::onEditor(){
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

    if (!isStatic) {
        ImGui::SeparatorText("Tunneling Prevention");

        ImGui::Checkbox("Velocity Clamping", &useVelocityClamping);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Limits speed each frame so the object cannot travel\n"
                              "more than N diameters in one frame.\n"
                              "Primary defence: keeps objects from skipping past walls.");
        if (useVelocityClamping) {
            ImGui::SetNextItemWidth(130.f);
            ImGui::DragFloat("Max Diameters/Frame##vcd", &velocityClampDiameters,
                             0.05f, 0.1f, 10.f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("1.0 = cannot move more than one own-diameter per frame.\n"
                                  "Lower = tighter cap, less risk of tunneling.");
        }

        ImGui::Spacing();
        ImGui::Checkbox("Is Fast Moving (Swept AABB)", &isFastMoving);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Expands the broad-phase AABB to also cover where this\n"
                              "object will be next frame (current + velocity * dt).\n"
                              "Ensures fast projectiles are never missed by the\n"
                              "broad phase even when approaching a thin obstacle.");
    }
}

void ComponentRigidbody::onSave(std::string& outJson) const{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("mass", mass, a);
    doc.AddMember("isStatic", isStatic, a);
    doc.AddMember("restitution", restitution, a);
    doc.AddMember("linearDamping", linearDamping, a);
    doc.AddMember("useGravity", useGravity, a);
    doc.AddMember("gravityScale", gravityScale, a);
    doc.AddMember("useVelocityClamping", useVelocityClamping, a);
    doc.AddMember("velocityClampDiameters", velocityClampDiameters, a);
    doc.AddMember("isFastMoving", isFastMoving, a);
    Value vel(kArrayType);
    vel.PushBack(velocity.x, a).PushBack(velocity.y, a).PushBack(velocity.z, a);
    doc.AddMember("velocity", vel, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentRigidbody::onLoad(const std::string& jsonStr){
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("mass")) mass = doc["mass"].GetFloat();
    if (doc.HasMember("isStatic")) isStatic = doc["isStatic"].GetBool();
    if (doc.HasMember("restitution")) restitution = doc["restitution"].GetFloat();
    if (doc.HasMember("linearDamping")) linearDamping = doc["linearDamping"].GetFloat();
    if (doc.HasMember("useGravity")) useGravity = doc["useGravity"].GetBool();
    if (doc.HasMember("gravityScale")) gravityScale = doc["gravityScale"].GetFloat();
    if (doc.HasMember("useVelocityClamping")) useVelocityClamping = doc["useVelocityClamping"].GetBool();
    if (doc.HasMember("velocityClampDiameters")) velocityClampDiameters = doc["velocityClampDiameters"].GetFloat();
    if (doc.HasMember("isFastMoving")) isFastMoving = doc["isFastMoving"].GetBool();
    if (doc.HasMember("velocity")) {
        const auto& v = doc["velocity"];
        velocity = { v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat() };
    }
}
