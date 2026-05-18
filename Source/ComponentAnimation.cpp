#include "Globals.h"
#include "ComponentAnimation.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ResourceAnimation.h"
#include "debug_draw.hpp"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <imgui.h>

using namespace rapidjson;

ComponentAnimation::ComponentAnimation(GameObject* owner)
    : Component(owner)
{
}

// ?? update ?????????????????????????????????????????????????????
void ComponentAnimation::update(float dt) {
    m_controller.update(dt);
    if (m_controller.isPlaying())
        applyAnimToTree(owner);
    if (m_debugBones && owner) {
        Vector3 rootPos = owner->getTransform()->
            getGlobalMatrix().Translation();
        debugDrawBones(owner, rootPos);
    }
}

// ?? walk tree and write transforms ????????????????????????????
void ComponentAnimation::applyAnimToTree(GameObject* node) {
    if (!node) return;

    ComponentTransform* t = node->getTransform();
    if (t) {
        Vector3    pos = t->position;
        Quaternion rot = t->rotation;
        Vector3    scale = t->scale;

        if (m_controller.getTransform(node->getName(), pos, rot, scale)) {
            t->position = pos;
            t->rotation = rot;
            t->scale = scale;
            t->markDirty();
        }
    }

    for (GameObject* child : node->getChildren())
        applyAnimToTree(child);
}

void ComponentAnimation::play(UID animUID, bool loop) {
    m_controller.play(animUID, loop);
}

void ComponentAnimation::stop() {
    m_controller.stop();
}

// ?? debug bone draw ????????????????????????????????????????????
void ComponentAnimation::debugDrawBones(
    GameObject* node, const Vector3& parentPos) const
{
    if (!node) return;

    Vector3 nodePos = node->getTransform()->getGlobalMatrix().Translation();

    // Draw line from parent to this bone
    if (node != owner) {
        ddVec3 from = { parentPos.x, parentPos.y, parentPos.z };
        ddVec3 to = { nodePos.x,   nodePos.y,   nodePos.z };
        dd::line(from, to, dd::colors::Cyan);
    }

    // FIX: ddConvert() already returns the pointer dd::axisTriad needs
    // (a const float* into a 4x4 matrix). Calling .m[0] on it treats
    // a float* as a struct, causing both compiler errors.
    // Pass the result of ddConvert() directly.
    Matrix world = node->getTransform()->getGlobalMatrix();
    dd::axisTriad(ddConvert(world), 0.0f, 0.05f, 0.05f);

    for (GameObject* child : node->getChildren())
        debugDrawBones(child, nodePos);
}

// ?? Editor UI ??????????????????????????????????????????????????
void ComponentAnimation::onEditor() {
    if (!ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    bool playing = m_controller.isPlaying();
    ImGui::Text("State: %s", playing ? "Playing" : "Stopped");

    if (playing) {
        float cur = m_controller.getCurrentTime();
        ResourceAnimation* res =
            app->getResources()->RequestAnimation(m_controller.getAnimUID());
        float dur = res ? res->getDuration() : 1.0f;
        ImGui::ProgressBar(cur / dur, ImVec2(-1, 0),
            (std::to_string(cur).substr(0, 4) +
                " / " + std::to_string(dur).substr(0, 4) +
                " s").c_str());
    }

    ImGui::Separator();
    ImGui::Text("Available clips:");

    for (size_t i = 0; i < m_availableAnims.size(); ++i) {
        UID uid = m_availableAnims[i];
        const std::string& name =
            (i < m_availableAnimNames.size())
            ? m_availableAnimNames[i]
            : "Anim_" + std::to_string(i);

        bool active = (playing && m_controller.getAnimUID() == uid);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button,
                ImVec4(0.2f, 0.6f, 0.2f, 1.f));

        ImGui::PushID((int)uid);
        if (ImGui::Button(active ? (" > " + name).c_str() : name.c_str(),
            ImVec2(-1, 0)))
            play(uid, true);
        ImGui::PopID();

        if (active) ImGui::PopStyleColor();
    }

    if (playing && ImGui::Button("[ ]  Stop"))
        stop();

    ImGui::Checkbox("Debug Draw Bones", &m_debugBones);
}

// ?? Serialization ??????????????????????????????????????????????
void ComponentAnimation::onSave(std::string& outJson) const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("AnimUID", m_controller.getAnimUID(), a);
    doc.AddMember("IsPlaying", m_controller.isPlaying(), a);
    Value arr(kArrayType);
    for (UID uid : m_availableAnims) arr.PushBack(uid, a);
    doc.AddMember("AvailableAnims", arr, a);
    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentAnimation::onLoad(const std::string& json) {
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()) return;
    if (doc.HasMember("AvailableAnims")) {
        m_availableAnims.clear();
        for (auto& v : doc["AvailableAnims"].GetArray())
            m_availableAnims.push_back(v.GetUint64());
    }
    UID  uid = doc.HasMember("AnimUID") ? doc["AnimUID"].GetUint64() : 0;
    bool was = doc.HasMember("IsPlaying") && doc["IsPlaying"].GetBool();
    if (uid && was) play(uid, true);
}