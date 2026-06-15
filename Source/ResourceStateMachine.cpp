#include "Globals.h"
#include "ResourceStateMachine.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/prettywriter.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

ResourceStateMachine::ResourceStateMachine(UID uid)
    : ResourceBase(uid, Type::StateMachine){}


bool ResourceStateMachine::LoadInMemory(){
    return Load(libraryFile);
}

void ResourceStateMachine::UnloadFromMemory(){
    clips.clear();
    states.clear();
    transitions.clear();
    defaultState = HashString{};
}


const SMClip* ResourceStateMachine::FindClip(const HashString& name) const{
    for (const auto& c : clips)
        if (c.name == name) return &c;
    return nullptr;
}

const SMState* ResourceStateMachine::FindState(const HashString& name) const{
    for (const auto& s : states)
        if (s.name == name) return &s;
    return nullptr;
}

int ResourceStateMachine::FindStateIndex(const HashString& name) const{
    for (int i = 0; i < (int)states.size(); ++i)
        if (states[i].name == name) return i;
    return -1;
}


bool ResourceStateMachine::Save(const std::string& path) const{
    Document doc;
    doc.SetObject();
    auto& a = doc.GetAllocator();

    doc.AddMember("Version", 1, a);
    doc.AddMember("DefaultState", Value(defaultState.str.c_str(), a), a);

    Value clipArr(kArrayType);
    for (const auto& c : clips){
        Value obj(kObjectType);
        obj.AddMember("Name", Value(c.name.str.c_str(), a), a);
        obj.AddMember("AnimationUID", c.animationUID, a);
        obj.AddMember("Loop", c.loop, a);
        clipArr.PushBack(obj, a);
    }
    doc.AddMember("Clips", clipArr, a);

    Value stateArr(kArrayType);
    for (const auto& s : states){
        Value obj(kObjectType);
        obj.AddMember("Name", Value(s.name.str.c_str(), a), a);
        obj.AddMember("Clip", Value(s.clipName.str.c_str(), a), a);
        stateArr.PushBack(obj, a);
    }
    doc.AddMember("States", stateArr, a);

    Value transArr(kArrayType);
    for (const auto& t : transitions){
        Value obj(kObjectType);
        obj.AddMember("Source", Value(t.source.str.c_str(), a), a);
        obj.AddMember("Target", Value(t.target.str.c_str(), a), a);
        obj.AddMember("Trigger", Value(t.trigger.str.c_str(), a), a);
        obj.AddMember("BlendMs", t.interpolationMs, a);
        transArr.PushBack(obj, a);
    }
    doc.AddMember("Transitions", transArr, a);

    StringBuffer sb;
    PrettyWriter<StringBuffer> writer(sb);
    doc.Accept(writer);
    return app->getFileSystem()->Save(path.c_str(), sb.GetString(), (unsigned)sb.GetSize());
}


bool ResourceStateMachine::Load(const std::string& path){
    clips.clear();
    states.clear();
    transitions.clear();
    defaultState = HashString{};

    char* buf = nullptr;
    unsigned size = app->getFileSystem()->Load(path.c_str(), &buf);
    if (!buf || size == 0){
        LOG("ResourceStateMachine: could not read '%s'", path.c_str());
        return false;
    }

    Document doc;
    doc.Parse(buf, size);
    delete[] buf;

    if (doc.HasParseError()){
        LOG("ResourceStateMachine: JSON parse error in '%s'", path.c_str());
        return false;
    }

    if (doc.HasMember("DefaultState") && doc["DefaultState"].IsString())
        defaultState = std::string(doc["DefaultState"].GetString());

    if (doc.HasMember("Clips") && doc["Clips"].IsArray()){
        const Value& arr = doc["Clips"];
        clips.reserve(arr.Size());
        for (SizeType i = 0; i < arr.Size(); ++i){
            const Value& v = arr[i];
            if (!v.HasMember("Name") || !v["Name"].IsString()){
                LOG("ResourceStateMachine: Clips[%u] missing Name — skipped", i);
                continue;
            }
            SMClip c;
            c.name = std::string(v["Name"].GetString());
            c.animationUID = v.HasMember("AnimationUID") ? v["AnimationUID"].GetUint64() : 0;
            c.loop = !v.HasMember("Loop") || v["Loop"].GetBool();
            clips.push_back(std::move(c));
        }
    }

    if (doc.HasMember("States") && doc["States"].IsArray()){
        const Value& arr = doc["States"];
        states.reserve(arr.Size());
        for (SizeType i = 0; i < arr.Size(); ++i){
            const Value& v = arr[i];
            if (!v.HasMember("Name") || !v["Name"].IsString()){
                LOG("ResourceStateMachine: States[%u] missing Name — skipped", i);
                continue;
            }
            SMState s;
            s.name = std::string(v["Name"].GetString());
            s.clipName = (v.HasMember("Clip") && v["Clip"].IsString())
                       ? std::string(v["Clip"].GetString())
                       : std::string{};

            if (!s.clipName.empty() && !FindClip(s.clipName))
                LOG("ResourceStateMachine: state '%s' references unknown clip '%s'",
                    s.name.str.c_str(), s.clipName.str.c_str());

            states.push_back(std::move(s));
        }
    }

    if (doc.HasMember("Transitions") && doc["Transitions"].IsArray()){
        const Value& arr = doc["Transitions"];
        transitions.reserve(arr.Size());
        for (SizeType i = 0; i < arr.Size(); ++i){
            const Value& v = arr[i];
            if (!v.HasMember("Source") || !v["Source"].IsString() ||
                !v.HasMember("Target") || !v["Target"].IsString()){
                LOG("ResourceStateMachine: Transitions[%u] missing Source/Target — skipped", i);
                continue;
            }
            SMTransition t;
            t.source = std::string(v["Source"].GetString());
            t.target = std::string(v["Target"].GetString());
            t.trigger = (v.HasMember("Trigger") && v["Trigger"].IsString())
                              ? std::string(v["Trigger"].GetString())
                              : std::string{};
            t.interpolationMs = v.HasMember("BlendMs") ? v["BlendMs"].GetUint() : 200u;

            if (!FindState(t.source)){
                LOG("ResourceStateMachine: Transitions[%u] unknown source state '%s' — skipped",
                    i, t.source.str.c_str());
                continue;
            }
            if (!FindState(t.target)){
                LOG("ResourceStateMachine: Transitions[%u] unknown target state '%s' — skipped",
                    i, t.target.str.c_str());
                continue;
            }
            transitions.push_back(std::move(t));
        }
    }

    return true;
}


static constexpr ImGuiTableFlags kTableFlags =
    ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;

void ResourceStateMachine::DrawInspector(){

    if (ImGui::CollapsingHeader("Clips", ImGuiTreeNodeFlags_DefaultOpen)){
        int removeIdx = -1;
        if (ImGui::BeginTable("##smclips", 4, kTableFlags)){
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Animation UID", ImGuiTableColumnFlags_WidthStretch, 3.0f);
            ImGui::TableSetupColumn("Loop", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 22.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)clips.size(); ++i){
                ImGui::TableNextRow();
                ImGui::PushID(i);
                auto& clip = clips[i];

                ImGui::TableSetColumnIndex(0);
                char nameBuf[128] = {};
                strncpy_s(nameBuf, clip.name.str.c_str(), sizeof(nameBuf) - 1);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##n", nameBuf, sizeof(nameBuf)))
                    clip.name = std::string(nameBuf);

                ImGui::TableSetColumnIndex(1);
                char uidBuf[24] = {};
                sprintf_s(uidBuf, "%llu", static_cast<unsigned long long>(clip.animationUID));
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##u", uidBuf, sizeof(uidBuf), ImGuiInputTextFlags_CharsDecimal))
                    clip.animationUID = static_cast<UID>(strtoull(uidBuf, nullptr, 10));

                ImGui::TableSetColumnIndex(2);
                ImGui::Checkbox("##l", &clip.loop);

                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton("X")) removeIdx = i;

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (removeIdx >= 0) clips.erase(clips.begin() + removeIdx);
        if (ImGui::Button("+ Add Clip")) clips.push_back({});
    }

    if (ImGui::CollapsingHeader("States", ImGuiTreeNodeFlags_DefaultOpen)){
        std::vector<const char*> clipNames;
        clipNames.reserve(clips.size());
        for (const auto& c : clips) clipNames.push_back(c.name.str.c_str());

        int removeIdx = -1;
        if (ImGui::BeginTable("##smstates", 4, kTableFlags)){
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Clip", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Default", ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 22.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)states.size(); ++i){
                ImGui::TableNextRow();
                ImGui::PushID(i);
                auto& state = states[i];

                ImGui::TableSetColumnIndex(0);
                char nameBuf[128] = {};
                strncpy_s(nameBuf, state.name.str.c_str(), sizeof(nameBuf) - 1);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##n", nameBuf, sizeof(nameBuf))){
                    bool wasDefault = (defaultState == state.name);
                    state.name = std::string(nameBuf);
                    if (wasDefault) defaultState = state.name;
                }

                ImGui::TableSetColumnIndex(1);
                {
                    int clipIdx = -1;
                    for (int j = 0; j < (int)clips.size(); ++j)
                        if (clips[j].name == state.clipName){ clipIdx = j; break; }
                    const char* preview = (clipIdx >= 0) ? clipNames[clipIdx] : "(none)";
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::BeginCombo("##c", preview)){
                        for (int j = 0; j < (int)clips.size(); ++j){
                            bool sel = (j == clipIdx);
                            if (ImGui::Selectable(clipNames[j], sel)) state.clipName = clips[j].name.str;
                            if (sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }

                ImGui::TableSetColumnIndex(2);
                {
                    bool isDefault = (defaultState == state.name);
                    if (ImGui::Checkbox("##d", &isDefault)){
                        if (isDefault) defaultState = state.name;
                        else if (defaultState == state.name) defaultState = HashString{};
                    }
                }

                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton("X")) removeIdx = i;

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (removeIdx >= 0){
            if (defaultState == states[removeIdx].name) defaultState = HashString{};
            states.erase(states.begin() + removeIdx);
        }
        if (ImGui::Button("+ Add State")) states.push_back({});
    }

    if (ImGui::CollapsingHeader("Transitions", ImGuiTreeNodeFlags_DefaultOpen)){
        std::vector<const char*> stateNames;
        stateNames.reserve(states.size());
        for (const auto& s : states) stateNames.push_back(s.name.str.c_str());

        auto drawStateCombo = [&](const char* id, HashString& field){
            int idx = -1;
            for (int j = 0; j < (int)states.size(); ++j)
                if (states[j].name == field){ idx = j; break; }
            const char* preview = (idx >= 0) ? stateNames[idx] : "(none)";
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo(id, preview)){
                for (int j = 0; j < (int)states.size(); ++j){
                    bool sel = (j == idx);
                    if (ImGui::Selectable(stateNames[j], sel)) field = states[j].name.str;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        };

        int removeIdx = -1;
        if (ImGui::BeginTable("##smtrans", 5, kTableFlags)){
            ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Trigger", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Blend ms", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 22.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)transitions.size(); ++i){
                ImGui::TableNextRow();
                ImGui::PushID(i);
                auto& tr = transitions[i];

                ImGui::TableSetColumnIndex(0); drawStateCombo("##src", tr.source);
                ImGui::TableSetColumnIndex(1); drawStateCombo("##tgt", tr.target);

                ImGui::TableSetColumnIndex(2);
                char trigBuf[128] = {};
                strncpy_s(trigBuf, tr.trigger.str.c_str(), sizeof(trigBuf) - 1);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##tr", trigBuf, sizeof(trigBuf)))
                    tr.trigger = std::string(trigBuf);

                ImGui::TableSetColumnIndex(3);
                int blendMs = static_cast<int>(tr.interpolationMs);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##b", &blendMs, 0, 2000))
                    tr.interpolationMs = static_cast<uint32_t>(blendMs);

                ImGui::TableSetColumnIndex(4);
                if (ImGui::SmallButton("X")) removeIdx = i;

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (removeIdx >= 0) transitions.erase(transitions.begin() + removeIdx);
        if (ImGui::Button("+ Add Transition")) transitions.push_back({});
    }
}
