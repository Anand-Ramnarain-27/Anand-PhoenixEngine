#include "Globals.h"
#include "ComponentAnimation.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ResourceMesh.h"
#include "ModuleResources.h"
#include "ResourceAnimation.h"
#include "Application.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

ComponentAnimation::ComponentAnimation(GameObject* owner) : Component(owner) {}

void ComponentAnimation::setAnimationList(const std::vector<UID>& uids) {
    m_animUIDs.clear();
    m_animNames.clear();
    for (UID uid : uids) {
        m_animUIDs.push_back(uid);
        auto* anim = app->getResources()->RequestAnimation(uid);
        if (anim) {
            std::string name = anim->getAnimName();
            m_animNames.push_back(name.empty() ? ("Anim_" + std::to_string(m_animUIDs.size() - 1)) : name);
            app->getResources()->ReleaseResource(anim);
        } else {
            m_animNames.push_back("Anim_" + std::to_string(m_animUIDs.size() - 1));
        }
    }
}

void ComponentAnimation::OnPlay(UID uid, bool loop) {
    m_controller.Play(uid, loop);
}

void ComponentAnimation::OnStop() {
    m_controller.Stop();
}

void ComponentAnimation::update(float deltaTime) {
    if (!m_controller.isPlaying()) return;
    m_controller.Update(deltaTime);

    for (auto* child : owner->getChildren())
        applyAnimation(child);

    // Diagnostic: log morph weights once per second so we can see what the animation is driving.
    m_logTimer += deltaTime;
    if (m_logTimer >= 1.f) {
        m_logTimer = 0.f;
        std::function<void(GameObject*)> logWeights = [&](GameObject* go) {
            if (auto* cm = go->getComponent<ComponentMesh>()) {
                const auto& entries = cm->getEntries();
                if (!entries.empty() && entries[0].meshRes) {
                    const uint32_t n = entries[0].meshRes->getNumMorphTargets();
                    if (n > 0) {
                        const float* w = cm->getMorphWeights();
                        std::string ws;
                        for (uint32_t i = 0; i < n && i < 8; ++i) {
                            ws += std::to_string(w[i]);
                            if (i + 1 < n && i + 1 < 8) ws += ", ";
                        }
                        LOG("ComponentAnimation '%s': node='%s' morph weights=[%s]",
                            owner->getName().c_str(), go->getName().c_str(), ws.c_str());
                    }
                }
            }
            for (auto* child : go->getChildren()) logWeights(child);
        };
        for (auto* child : owner->getChildren()) logWeights(child);
    }
}

void ComponentAnimation::applyAnimation(GameObject* go) {
    const char* nodeName = go->getName().c_str();

    auto* t = go->getTransform();
    Vector3 pos = t->position;
    Quaternion rot = t->rotation;
    if (m_controller.GetTransform(nodeName, pos, rot)) {
        t->position = pos;
        t->rotation = rot;
        t->markDirty();
    }

    auto* meshComp = go->getComponent<ComponentMesh>();
    if (meshComp) {
        const auto& entries = meshComp->getEntries();
        if (!entries.empty() && entries[0].meshRes != nullptr) {
            const uint32_t numTargets = entries[0].meshRes->getNumMorphTargets();
            if (numTargets > 0) {
                float weights[ComponentMesh::MAX_MORPH_WEIGHTS] = {};
                if (m_controller.GetMorphWeights(nodeName, weights, numTargets)) {
                    for (uint32_t i = 0; i < numTargets; ++i)
                        meshComp->setMorphWeight((int)i, weights[i]);
                }
            }
        }
    }

    for (auto* child : go->getChildren())
        applyAnimation(child);
}

void ComponentAnimation::onEditor() {
    // Dropdown: pick animation from list
    int currentIdx = -1;
    for (int i = 0; i < (int)m_animUIDs.size(); ++i) {
        if (m_animUIDs[i] == m_controller.Resource) { currentIdx = i; break; }
    }
    const char* preview = (currentIdx >= 0 && currentIdx < (int)m_animNames.size())
                          ? m_animNames[currentIdx].c_str() : "None";

    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##animsel", preview)) {
        for (int i = 0; i < (int)m_animUIDs.size(); ++i) {
            bool selected = (i == currentIdx);
            if (ImGui::Selectable(m_animNames[i].c_str(), selected))
                m_controller.Play(m_animUIDs[i], m_controller.Loop);
            if (selected) ImGui::SetItemDefaultFocus();
        }
        if (m_animUIDs.empty()) { ImGui::TextDisabled("No animations loaded"); }
        ImGui::EndCombo();
    }
    ImGui::Spacing();

    // Play / Stop
    if (m_controller.isPlaying()) {
        if (ImGui::Button("Stop")) m_controller.Stop();
    } else {
        if (ImGui::Button("Play")) {
            UID uid = m_controller.Resource;
            if (uid == 0 && !m_animUIDs.empty()) uid = m_animUIDs[0];
            if (uid != 0) m_controller.Play(uid, m_controller.Loop);
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &m_controller.Loop);

    // Time slider
    float duration = 0.f;
    if (m_controller.Resource != 0) {
        auto* anim = app->getResources()->RequestAnimation(m_controller.Resource);
        if (anim) { duration = anim->getDuration(); app->getResources()->ReleaseResource(anim); }
    }
    if (duration > 0.f) {
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##animtime", &m_controller.CurrentTime, 0.f, duration, "%.2f s");
        ImGui::SameLine(0, 4); ImGui::TextDisabled("/ %.2f", duration);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Checkbox("Draw Bones",       &m_drawBones);
    ImGui::Checkbox("Draw Axis Triads", &m_drawAxisTriads);
}

void ComponentAnimation::onDrawGizmos() {
    if (!m_drawBones && !m_drawAxisTriads) return;

    auto draw = [&](auto& self, GameObject* go) -> void {
        auto* t = go->getTransform();
        if (!t) return;

        if (m_drawBones && go->getParent() && go->getParent() != owner) {
            Vector3 from = go->getParent()->getTransform()->getGlobalMatrix().Translation();
            Vector3 to   = t->getGlobalMatrix().Translation();
            ddVec3 f  = { from.x, from.y, from.z };
            ddVec3 tt = { to.x,   to.y,   to.z   };
            dd::line(f, tt, dd::colors::Yellow);
        }

        if (m_drawAxisTriads) {
            Matrix world = t->getGlobalMatrix();
            dd::axisTriad(world.m[0], 0.f, 0.1f);
        }

        for (auto* child : go->getChildren())
            self(self, child);
    };

    for (auto* child : owner->getChildren())
        draw(draw, child);
}

void ComponentAnimation::onSave(std::string& outJson) const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("animUID",    Value(static_cast<uint64_t>(m_controller.Resource)), a);
    doc.AddMember("loop",       Value(m_controller.Loop), a);
    doc.AddMember("playing",    Value(m_controller.isPlaying()), a);
    doc.AddMember("currentTime",Value(m_controller.CurrentTime), a);

    // Save full animation UID list so it survives scene save/load
    Value uids(kArrayType);
    for (UID uid : m_animUIDs) uids.PushBack(Value(static_cast<uint64_t>(uid)), a);
    doc.AddMember("animUIDs", uids, a);

    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentAnimation::onLoad(const std::string& json) {
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()) { LOG("ComponentAnimation: JSON parse error"); return; }

    // Restore animation list
    if (doc.HasMember("animUIDs") && doc["animUIDs"].IsArray()) {
        std::vector<UID> uids;
        for (const auto& v : doc["animUIDs"].GetArray())
            uids.push_back(static_cast<UID>(v.GetUint64()));
        setAnimationList(uids);
    }

    UID uid  = doc.HasMember("animUID") ? static_cast<UID>(doc["animUID"].GetUint64()) : 0;
    bool loop    = doc.HasMember("loop")    ? doc["loop"].GetBool()    : false;
    bool playing = doc.HasMember("playing") ? doc["playing"].GetBool() : false;
    float time   = doc.HasMember("currentTime") ? doc["currentTime"].GetFloat() : 0.f;

    if (uid != 0) {
        m_controller.Play(uid, loop);
        if (!playing) m_controller.Stop();
        m_controller.CurrentTime = time;
    }
}
