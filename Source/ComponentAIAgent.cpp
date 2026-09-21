#include "Globals.h"
#include "ComponentAIAgent.h"
#include "ComponentTransform.h"
#include "GameObject.h"
#include "SceneGraph.h"
#include "Application.h"
#include "RuntimeCore.h"
#include "NavigationSystem.h"
#include "SteeringBehaviors.h"
#include <imgui.h>
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

namespace {
    constexpr float kRepathThresholdSq = 1.f; // re-request a path once the goal drifts > 1 unit
}

ComponentAIAgent::ComponentAIAgent(GameObject* owner) : Component(owner){}

void ComponentAIAgent::updateBehavior(){
    GameObject* target = nullptr;
    if (SceneGraph* scene = app->getRuntimeCore()->getActiveModuleScene())
        target = scene->findGameObjectByName(targetName);

    if (target && target->getTransform()){
        float dist = (target->getTransform()->position - owner->getTransform()->position).Length();
        if (dist <= detectionRange){
            behavior = Behavior::Chase;
            return;
        }
    }

    behavior = patrolPoints.empty() ? Behavior::Idle : Behavior::Patrol;
}

void ComponentAIAgent::requestPathTo(const Vector3& goal){
    if ((goal - lastGoal).LengthSquared() < kRepathThresholdSq && !currentPath.empty())
        return;

    NavigationSystem* nav = app->getRuntimeCore()->getNavigationSystem();
    if (!nav) return;

    std::vector<Vector3> path;
    if (nav->FindPath(owner->getTransform()->position, goal, path)){
        currentPath = std::move(path);
        pathIndex = 0;
        lastGoal = goal;
    }
}

void ComponentAIAgent::update(float dt){
    updateBehavior();

    ComponentTransform* t = owner->getTransform();
    if (!t) return;

    if (behavior == Behavior::Idle){
        currentPath.clear();
        velocity = Vector3::Zero;
        return;
    }

    Vector3 goal;
    if (behavior == Behavior::Chase){
        SceneGraph* scene = app->getRuntimeCore()->getActiveModuleScene();
        GameObject* target = scene ? scene->findGameObjectByName(targetName) : nullptr;
        if (!target || !target->getTransform()) return;
        goal = target->getTransform()->position;
    } else { // Patrol
        if (patrolTargetIndex >= (int)patrolPoints.size()) patrolTargetIndex = 0;
        goal = patrolPoints[patrolTargetIndex];
    }

    requestPathTo(goal);

    SteeringBehaviors::SteeringParams params;
    params.maxSpeed = maxSpeed;
    params.maxAccel = maxAccel;
    params.arriveRadius = arriveRadius;

    Vector3 desired = SteeringBehaviors::PathFollow(t->position, currentPath, &pathIndex, params);
    velocity = SteeringBehaviors::ApplySteering(velocity, desired, params, dt);

    if (velocity.LengthSquared() > 1e-6f){
        t->position += velocity * dt;
        t->markDirty();
    }

    if (behavior == Behavior::Patrol && pathIndex >= (int)currentPath.size() &&
        (t->position - goal).Length() < arriveRadius){
        patrolTargetIndex = (patrolTargetIndex + 1) % (int)patrolPoints.size();
        currentPath.clear();
        lastGoal = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    }
}

void ComponentAIAgent::onEditor(){
    const char* behaviorNames[] = { "Idle", "Patrol", "Chase" };
    ImGui::SeparatorText("AI Agent");
    ImGui::Text("Behavior: %s", behaviorNames[(int)behavior]);
    ImGui::Text("Path waypoints remaining: %d", (int)currentPath.size() - pathIndex);

    ImGui::SeparatorText("Movement");
    ImGui::DragFloat("Max Speed", &maxSpeed, 0.1f, 0.1f, 50.f, "%.2f");
    ImGui::DragFloat("Max Acceleration", &maxAccel, 0.1f, 0.1f, 100.f, "%.2f");
    ImGui::DragFloat("Arrive Radius", &arriveRadius, 0.05f, 0.05f, 20.f, "%.2f");

    ImGui::SeparatorText("Perception");
    char nameBuf[128];
    strncpy_s(nameBuf, targetName.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("Target Name", nameBuf, sizeof(nameBuf)))
        targetName = nameBuf;
    ImGui::DragFloat("Detection Range", &detectionRange, 0.1f, 0.f, 100.f, "%.2f");

    ImGui::SeparatorText("Patrol Points");
    for (int i = 0; i < (int)patrolPoints.size(); ++i){
        ImGui::PushID(i);
        float p[3] = { patrolPoints[i].x, patrolPoints[i].y, patrolPoints[i].z };
        if (ImGui::DragFloat3("##pp", p, 0.1f))
            patrolPoints[i] = { p[0], p[1], p[2] };
        ImGui::SameLine();
        if (ImGui::Button("Remove")){
            patrolPoints.erase(patrolPoints.begin() + i);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (ImGui::Button("Add Patrol Point At Current Position") && owner->getTransform())
        patrolPoints.push_back(owner->getTransform()->position);
}

void ComponentAIAgent::onSave(std::string& outJson) const{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("maxSpeed", maxSpeed, a);
    doc.AddMember("maxAccel", maxAccel, a);
    doc.AddMember("arriveRadius", arriveRadius, a);
    doc.AddMember("detectionRange", detectionRange, a);
    doc.AddMember("targetName", Value(targetName.c_str(), a), a);

    Value pts(kArrayType);
    for (const auto& p : patrolPoints){
        Value v(kArrayType);
        v.PushBack(p.x, a).PushBack(p.y, a).PushBack(p.z, a);
        pts.PushBack(v, a);
    }
    doc.AddMember("patrolPoints", pts, a);

    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentAIAgent::onLoad(const std::string& jsonStr){
    Document doc; doc.Parse(jsonStr.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("maxSpeed")) maxSpeed = doc["maxSpeed"].GetFloat();
    if (doc.HasMember("maxAccel")) maxAccel = doc["maxAccel"].GetFloat();
    if (doc.HasMember("arriveRadius")) arriveRadius = doc["arriveRadius"].GetFloat();
    if (doc.HasMember("detectionRange")) detectionRange = doc["detectionRange"].GetFloat();
    if (doc.HasMember("targetName")) targetName = doc["targetName"].GetString();
    if (doc.HasMember("patrolPoints")){
        patrolPoints.clear();
        const auto& arr = doc["patrolPoints"];
        for (SizeType i = 0; i < arr.Size(); ++i){
            const auto& v = arr[i];
            patrolPoints.push_back({ v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat() });
        }
    }
}
