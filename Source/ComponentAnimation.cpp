#include "Globals.h"
#include "ComponentAnimation.h"
#include "ResourceAnimation.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "Application.h"
#include "ModuleResources.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/writer.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

ComponentAnimation::ComponentAnimation(GameObject* o) : Component(o) {}

ComponentAnimation::~ComponentAnimation() {
    if (m_animRes) {
        app->getResources()->ReleaseResource(m_animRes);
        m_animRes = nullptr;
    }
}

void ComponentAnimation::Play(UID animUID, bool loop) {
    if (m_animRes) {
        app->getResources()->ReleaseResource(m_animRes);
        m_animRes = nullptr;
    }
    m_animUID = animUID;
    m_loop = loop;
    if (animUID == 0) return;

    // RequestResource returns ResourceAnimation* with ref bumped.
    m_animRes = static_cast<ResourceAnimation*>(
        app->getResources()->RequestResource(animUID));
    if (!m_animRes) {
        LOG("ComponentAnimation: failed to load anim UID=%llu", animUID);
        return;
    }
    // AnimImporter::Load is called by ModuleResources::CreateResourceFromUID.
    m_controller.Play(m_animRes, loop);
}

void ComponentAnimation::Stop() {
    m_controller.Stop();
}

void ComponentAnimation::update(float dtMs) {
    // Component::update receives elapsed ms from GameObject::update.
    m_controller.Update(dtMs * 0.001f);   // convert ms -> seconds
    updateNode(owner);
}

void ComponentAnimation::updateNode(GameObject* go) {
    if (!go) return;
    ComponentTransform* t = go->getTransform();
    if (t) {
        Vector3    pos = t->position;
        Quaternion rot = t->rotation;
        bool changed = m_controller.GetTransform(go->getName(), pos, rot);
        if (changed) {
            t->position = pos;
            t->rotation = rot;
            t->markDirty();
        }
        // Phase 3: push morph weights into ComponentMesh if present.
        /*if (auto* cm = go->getComponent<ComponentMesh>()) {
            uint32_t n = (uint32_t)cm->getMorphWeightCount();
            if (n > 0) {
                std::vector<float> w(n, 0.f);
                if (m_controller.GetMorphWeights(go->getName(), w.data(), n))
                    cm->setMorphWeights(w);
            }
        }*/
    }
    for (auto* child : go->getChildren())
        updateNode(child);
}

void ComponentAnimation::onEditor() {
    // Drawn by InspectorPanel::drawComponentAnimation().
}

void ComponentAnimation::onSave(std::string& outJson) const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("AnimUID", m_animUID, a);
    doc.AddMember("Loop", m_loop, a);
    StringBuffer sb; Writer<StringBuffer> w(sb); doc.Accept(w);
    outJson = sb.GetString();
}

void ComponentAnimation::onLoad(const std::string& json) {
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()) return;
    UID uid = doc.HasMember("AnimUID") ? doc["AnimUID"].GetUint64() : 0;
    bool lp = doc.HasMember("Loop") ? doc["Loop"].GetBool() : true;
    if (uid) Play(uid, lp);
}
