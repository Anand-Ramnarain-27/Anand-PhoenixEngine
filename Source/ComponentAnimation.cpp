#include "Globals.h"
#include "ComponentAnimation.h"
#include "StateMachineGraphEditor.h"
#include "ModuleFileSystem.h"
#include "AssetBrowserPanel.h"
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
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>

using namespace rapidjson;


static bool SampleTransform(const ResourceAnimation* anim, float timeSec,
                             const char* name, Vector3& pos, Quaternion& rot){
    if (!anim) return false;
    const auto& channels = anim->getChannels();
    const auto it = channels.find(name);
    if (it == channels.end()) return false;

    const ResourceAnimation::Channel& ch = it->second;

    if (ch.posCount > 0){
        const float* tf = ch.posTimeStamps.get();
        const float* tl = tf + ch.posCount;
        const float* up = std::upper_bound(tf, tl, timeSec);
        if (up == tf) pos = ch.positions[0];
        else if (up == tl) pos = ch.positions[ch.posCount - 1];
        else {
            int i = (int)(up - tf) - 1;
            float d = tf[i + 1] - tf[i];
            float lm = d > 0.f ? (timeSec - tf[i]) / d : 0.f;
            pos = Vector3::Lerp(ch.positions[i], ch.positions[i + 1], lm);
        }
    }

    if (ch.rotCount > 0){
        const float* tf = ch.rotTimeStamps.get();
        const float* tl = tf + ch.rotCount;
        const float* up = std::upper_bound(tf, tl, timeSec);
        if (up == tf) rot = ch.rotations[0];
        else if (up == tl) rot = ch.rotations[ch.rotCount - 1];
        else {
            int i = (int)(up - tf) - 1;
            float d = tf[i + 1] - tf[i];
            float lm = d > 0.f ? (timeSec - tf[i]) / d : 0.f;
            rot = Quaternion::Slerp(ch.rotations[i], ch.rotations[i + 1], lm);
        }
    }
    return true;
}

static bool SampleMorphWeights(const ResourceAnimation* anim, float timeSec,
                                const char* name, float* out, uint32_t count){
    if (!anim || !out || count == 0) return false;
    const ResourceAnimation::MorphChannel* mc = anim->getMorphChannel(name);
    if (!mc || mc->numTime == 0 || mc->numTargets == 0) return false;

    const uint32_t chTargets = std::min(mc->numTargets, count);
    const float* tf = mc->weightsTimes.get();
    const float* tl = tf + mc->numTime;
    const float* up = std::upper_bound(tf, tl, timeSec);

    if (up == tf){
        for (uint32_t i = 0; i < chTargets; ++i)
            out[i] = mc->weights[i];
    } else if (up == tl){
        const uint32_t k = mc->numTime - 1;
        for (uint32_t i = 0; i < chTargets; ++i)
            out[i] = mc->weights[k * mc->numTargets + i];
    } else {
        const int k = (int)(up - tf) - 1;
        const float d = tf[k + 1] - tf[k];
        const float lm = d > 0.f
            ? std::max(0.f, std::min(1.f, (timeSec - tf[k]) / d))
            : 0.f;
        for (uint32_t i = 0; i < chTargets; ++i){
            out[i] = mc->weights[k * mc->numTargets + i] * (1.f - lm)
                   + mc->weights[(k + 1) * mc->numTargets + i] * lm;
        }
    }
    return true;
}


ComponentAnimation::ComponentAnimation(GameObject* owner) : Component(owner){}

ComponentAnimation::~ComponentAnimation(){
    clearLayers();
}


void ComponentAnimation::freeLayerChain(AnimLayer* head){
    while (head){
        AnimLayer* next = head->next;
        if (head->anim) app->getResources()->ReleaseResource(head->anim);
        delete head;
        head = next;
    }
}

void ComponentAnimation::clearLayers(){
    freeLayerChain(m_layerHead);
    m_layerHead = nullptr;
}

void ComponentAnimation::pushLayer(UID animUID, float transitionTimeMs, bool loop){
    ResourceAnimation* anim = app->getResources()->RequestAnimation(animUID);
    if (!anim){
        LOG("ComponentAnimation::pushLayer: failed to load animation uid=%llu", animUID);
        return;
    }
    AnimLayer* layer = new AnimLayer();
    layer->anim = anim;
    layer->currentTimeMs = 0.f;
    layer->fadeTimeMs = 0.f;
    layer->transitionTimeMs = transitionTimeMs;
    layer->loop = loop;
    layer->next = m_layerHead;
    m_layerHead = layer;
}


void ComponentAnimation::OnPlay(UID uid, bool loop){
    clearLayers();
    m_controller.Play(uid, loop);
}

void ComponentAnimation::OnStop(){
    clearLayers();
    m_controller.Stop();
}

void ComponentAnimation::OnPlay(){
    clearLayers();
    if (!m_stateMachine) return;

    const SMState* def = m_stateMachine->FindState(m_stateMachine->defaultState);
    if (!def) return;

    const SMClip* clip = m_stateMachine->FindClip(def->clipName);
    if (!clip || clip->animationUID == 0) return;

    m_activeState = m_stateMachine->defaultState;
    pushLayer(clip->animationUID, 0.f, clip->loop);
}

void ComponentAnimation::SendTrigger(const HashString& trigger){
    if (!m_stateMachine) return;

    for (const auto& tr : m_stateMachine->transitions){
        if (tr.source != m_activeState || tr.trigger != trigger) continue;

        const SMState* target = m_stateMachine->FindState(tr.target);
        if (!target) return;

        const SMClip* clip = m_stateMachine->FindClip(target->clipName);
        if (!clip || clip->animationUID == 0) return;

        pushLayer(clip->animationUID, (float)tr.interpolationMs, clip->loop);
        m_activeState = tr.target;
        return;
    }
}


void ComponentAnimation::LoadStateMachineFromPath(const std::string& path){
    if (path.empty()) return;
    auto sm = std::make_unique<ResourceStateMachine>(0);
    if (!sm->Load(path)){
        LOG("ComponentAnimation: failed to load SM from '%s'", path.c_str());
        return;
    }
    m_ownedStateMachine = std::move(sm);
    m_stateMachinePath = path;
    m_stateMachine = m_ownedStateMachine.get();
    OnPlay();
}


void ComponentAnimation::setAnimationList(const std::vector<UID>& uids){
    m_animUIDs.clear();
    m_animNames.clear();
    for (UID uid : uids){
        m_animUIDs.push_back(uid);
        auto* anim = app->getResources()->RequestAnimation(uid);
        if (anim){
            std::string name = anim->getAnimName();
            m_animNames.push_back(name.empty() ? ("Anim_" + std::to_string(m_animUIDs.size() - 1)) : name);
            app->getResources()->ReleaseResource(anim);
        } else {
            m_animNames.push_back("Anim_" + std::to_string(m_animUIDs.size() - 1));
        }
    }
}


bool ComponentAnimation::isOwnerMeshVisible() const{
    std::function<const ComponentMesh*(const GameObject*)> findMesh = [&](const GameObject* go) -> const ComponentMesh* {
        if (auto* cm = go->getComponent<ComponentMesh>()) return cm;
        for (auto* child : go->getChildren())
            if (auto* cm = findMesh(child)) return cm;
        return nullptr;
    };
    const ComponentMesh* cm = findMesh(owner);
    return !cm || cm->isVisible();
}

void ComponentAnimation::update(float deltaTime){
    const bool applyPose = isOwnerMeshVisible();
    if (m_layerHead){
        const float dtMs = deltaTime * 1000.f;

        for (AnimLayer* l = m_layerHead; l; l = l->next){
            l->currentTimeMs += dtMs * mSpeed;
            if (l->anim){
                const float durMs = l->anim->getDuration() * 1000.f;
                if (durMs > 0.f){
                    if (l->loop && l->currentTimeMs >= durMs)
                        l->currentTimeMs = std::fmod(l->currentTimeMs, durMs);
                    else if (!l->loop && l->currentTimeMs > durMs)
                        l->currentTimeMs = durMs;
                }
            }

            if (l->next)
                l->fadeTimeMs += dtMs;
        }

        if (m_layerHead->next &&
            m_layerHead->fadeTimeMs >= m_layerHead->transitionTimeMs){
            freeLayerChain(m_layerHead->next);
            m_layerHead->next = nullptr;
        }

        if (applyPose)
            for (auto* child : owner->getChildren())
                applyBlendedAnimation(child);

    } else {
        if (!m_controller.isPlaying()) return;
        m_controller.Update(deltaTime);

        if (applyPose)
            for (auto* child : owner->getChildren())
                applyAnimation(child);

        m_logTimer += deltaTime;
        if (m_logTimer >= 1.f){
            m_logTimer = 0.f;
            std::function<void(GameObject*)> logWeights = [&](GameObject* go){
                if (auto* cm = go->getComponent<ComponentMesh>()){
                    const auto& entries = cm->getEntries();
                    if (!entries.empty() && entries[0].meshRes){
                        const uint32_t n = entries[0].meshRes->getNumMorphTargets();
                        if (n > 0){
                            const float* w = cm->getMorphWeights();
                            std::string ws;
                            for (uint32_t i = 0; i < n && i < 8; ++i){
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
}


void ComponentAnimation::GetBlendedTransform(const char* name, AnimLayer* layer,
                                              Vector3& pos, Quaternion& rot) const{
    if (!layer) return;

    const float timeSec = layer->currentTimeMs / 1000.f;

    if (!layer->next){
        if (layer->anim)
            SampleTransform(layer->anim, timeSec, name, pos, rot);
        return;
    }

    GetBlendedTransform(name, layer->next, pos, rot);

    Vector3 thisPos = pos;
    Quaternion thisRot = rot;
    if (layer->anim)
        SampleTransform(layer->anim, timeSec, name, thisPos, thisRot);

    const float w = (layer->transitionTimeMs > 0.f)
        ? std::min(1.f, layer->fadeTimeMs / layer->transitionTimeMs)
        : 1.f;

    pos = Vector3::Lerp(pos, thisPos, w);

    if (rot.Dot(thisRot) < 0.f)
        thisRot = Quaternion(-thisRot.x, -thisRot.y, -thisRot.z, -thisRot.w);
    rot = Quaternion::Slerp(rot, thisRot, w);
}

void ComponentAnimation::GetBlendedMorphWeights(const char* name, AnimLayer* layer,
                                                  float* weights, uint32_t count) const{
    if (!layer || count == 0) return;

    const float timeSec = layer->currentTimeMs / 1000.f;

    if (!layer->next){
        if (layer->anim)
            SampleMorphWeights(layer->anim, timeSec, name, weights, count);
        return;
    }

    GetBlendedMorphWeights(name, layer->next, weights, count);

    float thisWeights[ComponentMesh::MAX_MORPH_WEIGHTS] = {};
    std::copy(weights, weights + count, thisWeights);
    if (layer->anim)
        SampleMorphWeights(layer->anim, timeSec, name, thisWeights, count);

    const float w = (layer->transitionTimeMs > 0.f)
        ? std::min(1.f, layer->fadeTimeMs / layer->transitionTimeMs)
        : 1.f;

    for (uint32_t i = 0; i < count; ++i)
        weights[i] += w * (thisWeights[i] - weights[i]);
}


void ComponentAnimation::applyAnimation(GameObject* go){
    const char* nodeName = go->getName().c_str();

    auto* t = go->getTransform();
    Vector3 pos = t->position;
    Quaternion rot = t->rotation;
    if (m_controller.GetTransform(nodeName, pos, rot)){
        t->position = pos;
        t->rotation = rot;
        t->markDirty();
    }

    auto* meshComp = go->getComponent<ComponentMesh>();
    if (meshComp){
        const auto& entries = meshComp->getEntries();
        if (!entries.empty() && entries[0].meshRes){
            const uint32_t numTargets = entries[0].meshRes->getNumMorphTargets();
            if (numTargets > 0){
                float weights[ComponentMesh::MAX_MORPH_WEIGHTS] = {};
                if (m_controller.GetMorphWeights(nodeName, weights, numTargets))
                    for (uint32_t i = 0; i < numTargets; ++i)
                        meshComp->setMorphWeight((int)i, weights[i]);
            }
        }
    }

    for (auto* child : go->getChildren())
        applyAnimation(child);
}

void ComponentAnimation::applyBlendedAnimation(GameObject* go){
    const char* name = go->getName().c_str();
    auto* t = go->getTransform();

    Vector3 pos = t->position;
    Quaternion rot = t->rotation;
    GetBlendedTransform(name, m_layerHead, pos, rot);
    t->position = pos;
    t->rotation = rot;
    t->markDirty();

    auto* meshComp = go->getComponent<ComponentMesh>();
    if (meshComp){
        const auto& entries = meshComp->getEntries();
        if (!entries.empty() && entries[0].meshRes){
            const uint32_t numTargets = entries[0].meshRes->getNumMorphTargets();
            if (numTargets > 0){
                float weights[ComponentMesh::MAX_MORPH_WEIGHTS] = {};
                GetBlendedMorphWeights(name, m_layerHead, weights, numTargets);
                for (uint32_t i = 0; i < numTargets; ++i)
                    meshComp->setMorphWeight((int)i, weights[i]);
            }
        }
    }

    for (auto* child : go->getChildren())
        applyBlendedAnimation(child);
}


void ComponentAnimation::onEditor(){
    ComponentAnimation* anim = this;
    const auto& uids = anim->getAnimationUIDs();
    AnimationController& ctrl = anim->getController();

    ImGui::SeparatorText("Clip");

    int currentIdx = -1;
    for (int i = 0; i < (int)uids.size(); ++i)
        if (uids[i] == ctrl.Resource){ currentIdx = i; break; }

    std::string preview = "None";
    if (currentIdx >= 0 && ctrl.Resource != 0){
        auto* r = app->getResources()->RequestAnimation(ctrl.Resource);
        if (r){ preview = r->getAnimName(); app->getResources()->ReleaseResource(r); }
        if (preview.empty()) preview = "Anim_" + std::to_string(currentIdx);
    }

    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##animclip", preview.c_str())){
        for (int i = 0; i < (int)uids.size(); ++i){
            auto* r = app->getResources()->RequestAnimation(uids[i]);
            std::string name = r ? r->getAnimName() : ("Anim_" + std::to_string(i));
            if (r) app->getResources()->ReleaseResource(r);
            bool sel = (i == currentIdx);
            if (ImGui::Selectable(name.c_str(), sel))
                ctrl.Play(uids[i], ctrl.Loop);
            if (sel) ImGui::SetItemDefaultFocus();
        }
        if (uids.empty()) ImGui::TextDisabled("No animations available");
        ImGui::EndCombo();
    }

    ImGui::SeparatorText("Playback");
    if (ctrl.isPlaying()){
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.f));
        if (ImGui::Button("Stop##animstop", ImVec2(60, 0))) ctrl.Stop();
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.1f, 1.f));
        if (ImGui::Button("Play##animplay", ImVec2(60, 0))){
            UID uid = ctrl.Resource;
            if (uid == 0 && !uids.empty()) uid = uids[0];
            if (uid != 0) ctrl.Play(uid, ctrl.Loop);
        }
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Loop##animloop", &ctrl.Loop);

    float duration = 0.f;
    if (ctrl.Resource != 0){
        auto* r = app->getResources()->RequestAnimation(ctrl.Resource);
        if (r){ duration = r->getDuration(); app->getResources()->ReleaseResource(r); }
    }
    if (duration > 0.f){
        ImGui::Spacing();
        ImGui::SetNextItemWidth(-60);
        if (ImGui::SliderFloat("##animscrub", &ctrl.CurrentTime, 0.f, duration, "%.2f s"))
            if (!ctrl.isPlaying()){ }
        ImGui::SameLine();
        ImGui::TextDisabled("%.1f s", duration);
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Debug");
    ImGui::Checkbox("Draw Bones", &anim->drawBones());
    ImGui::Checkbox("Draw Axis Triads", &anim->drawAxisTriads());

    anim->drawStateMachineSection();
}

void ComponentAnimation::drawStateMachineSection(){
    namespace fs = std::filesystem;
    ImGui::Spacing();
    ImGui::SeparatorText("State Machine");

    static char smBuf[512] = {};
    if (!m_stateMachinePath.empty() && smBuf[0] == '\0')
        strncpy_s(smBuf, m_stateMachinePath.c_str(), sizeof(smBuf) - 1);

    ImGui::SetNextItemWidth(-140.f);
    ImGui::InputTextWithHint("##smpath", "Assets/StateMachines/name.json", smBuf, sizeof(smBuf));

    if (ImGui::BeginDragDropTarget()){
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload(kDragAsset)){
            std::string dropped(static_cast<const char*>(pl->Data), pl->DataSize - 1);
            if (dropped.size() >= 5 &&
                dropped.compare(dropped.size() - 5, 5, ".json") == 0){
                strncpy_s(smBuf, dropped.c_str(), sizeof(smBuf) - 1);
                LoadStateMachineFromPath(dropped);
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::SameLine(0, 4);
    if (ImGui::Button("Load##sm", ImVec2(50, 0))){
        LoadStateMachineFromPath(std::string(smBuf));
    }

    ImGui::SameLine(0, 4);
    if (ImGui::Button("Pick##sm", ImVec2(50, 0)))
        ImGui::OpenPopup("##SMPicker");

    ImGui::SetNextWindowSize(ImVec2(340, 240), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("##SMPicker")){
        ImGui::TextDisabled("State Machine files  (double-click to load)");
        ImGui::Separator();
        static char pickerSearch[64] = "";
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##smpksearch", "Search...", pickerSearch, sizeof(pickerSearch));
        ImGui::Separator();

        std::string assetsRoot;
        if (app->getFileSystem()){
            assetsRoot = (fs::path(app->getFileSystem()->GetAssetsPath()) / "StateMachines").string();
        }

        std::string search = pickerSearch;
        std::transform(search.begin(), search.end(), search.begin(), ::tolower);
        bool any = false;
        try {
            if (fs::exists(assetsRoot)){
                for (const auto& entry : fs::directory_iterator(assetsRoot)){
                    if (!entry.is_regular_file()) continue;
                    if (entry.path().extension() != ".json") continue;
                    std::string name = entry.path().filename().string();
                    std::string nameLower = name;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    if (!search.empty() && nameLower.find(search) == std::string::npos) continue;

                    bool isCurrent = (entry.path().string() == m_stateMachinePath);
                    if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.f, 0.4f, 1.f));
                    bool clicked = ImGui::Selectable(("  [SM]  " + name).c_str(), isCurrent,
                                                     ImGuiSelectableFlags_AllowDoubleClick);
                    if (isCurrent) ImGui::PopStyleColor();
                    if (clicked && ImGui::IsMouseDoubleClicked(0)){
                        std::string fullPath = entry.path().string();
                        strncpy_s(smBuf, fullPath.c_str(), sizeof(smBuf) - 1);
                        LoadStateMachineFromPath(fullPath);
                        pickerSearch[0] = '\0';
                        ImGui::CloseCurrentPopup();
                    }
                    any = true;
                }
            }
        } catch (...){}
        if (!any){
            ImGui::TextDisabled("    No .json files found in Assets/StateMachines/");
        }
        ImGui::EndPopup();
    }

    if (m_stateMachine){
        ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "Active: %s",
            m_activeState.empty() ? "(none)" : m_activeState.str.c_str());
        int depth = getLayerCount();
        if (depth > 1)
            ImGui::TextDisabled("Blend layers: %d", depth);

        if (!m_graphEditor){
            m_graphEditor = std::make_unique<StateMachineGraphEditor>();
            std::string settingsPath = app->getFileSystem()->GetLibraryPath()
                + "smgraph_" + std::to_string(owner->getUID()) + ".ini";
            m_graphEditor->Init(settingsPath);
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Graph  (%d states, %d transitions)",
            (int)m_stateMachine->states.size(), (int)m_stateMachine->transitions.size());
        if (ImGui::BeginChild("##SMGraph", ImVec2(0.f, 260.f), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)){
            m_graphEditor->Draw(*m_stateMachine, &m_activeState);
        }
        ImGui::EndChild();
    } else {
        ImGui::TextDisabled("No SM loaded — drag a .json from the Asset Browser or use Pick");
    }
}


void ComponentAnimation::onDrawGizmos(){
    if (!m_drawBones && !m_drawAxisTriads) return;

    auto draw = [&](auto& self, GameObject* go) -> void {
        auto* t = go->getTransform();
        if (!t) return;

        if (m_drawBones && go->getParent() && go->getParent() != owner){
            Vector3 from = go->getParent()->getTransform()->getGlobalMatrix().Translation();
            Vector3 to = t->getGlobalMatrix().Translation();
            ddVec3 f = { from.x, from.y, from.z };
            ddVec3 tt = { to.x, to.y, to.z };
            dd::line(f, tt, dd::colors::Yellow);
        }

        if (m_drawAxisTriads){
            Matrix world = t->getGlobalMatrix();
            dd::axisTriad(world.m[0], 0.f, 0.5f);
        }

        for (auto* child : go->getChildren())
            self(self, child);
    };

    for (auto* child : owner->getChildren())
        draw(draw, child);
}


void ComponentAnimation::onSave(std::string& outJson) const{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("animUID", Value(static_cast<uint64_t>(m_controller.Resource)), a);
    doc.AddMember("loop", Value(m_controller.Loop), a);
    doc.AddMember("playing", Value(m_controller.isPlaying()), a);
    doc.AddMember("currentTime", Value(m_controller.CurrentTime), a);
    doc.AddMember("speed", Value(mSpeed), a);
    doc.AddMember("smPath", Value(m_stateMachinePath.c_str(), a), a);

    Value uids(kArrayType);
    for (UID uid : m_animUIDs) uids.PushBack(Value(static_cast<uint64_t>(uid)), a);
    doc.AddMember("animUIDs", uids, a);

    StringBuffer buf; Writer<StringBuffer> w(buf); doc.Accept(w);
    outJson = buf.GetString();
}

void ComponentAnimation::onLoad(const std::string& json){
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()){ LOG("ComponentAnimation: JSON parse error"); return; }

    if (doc.HasMember("animUIDs") && doc["animUIDs"].IsArray()){
        std::vector<UID> uids;
        for (const auto& v : doc["animUIDs"].GetArray())
            uids.push_back(static_cast<UID>(v.GetUint64()));
        setAnimationList(uids);
    }

    if (doc.HasMember("smPath") && doc["smPath"].IsString()){
        std::string path = doc["smPath"].GetString();
        if (!path.empty()) LoadStateMachineFromPath(path);
    }

    UID uid = doc.HasMember("animUID") ? static_cast<UID>(doc["animUID"].GetUint64()) : 0;
    bool loop = doc.HasMember("loop") ? doc["loop"].GetBool() : false;
    bool playing = doc.HasMember("playing") ? doc["playing"].GetBool() : false;
    float time = doc.HasMember("currentTime") ? doc["currentTime"].GetFloat() : 0.f;
    mSpeed = doc.HasMember("speed") ? doc["speed"].GetFloat() : 1.f;

    if (uid != 0){
        m_controller.Play(uid, loop);
        if (!playing) m_controller.Stop();
        m_controller.CurrentTime = time;
    }
}
