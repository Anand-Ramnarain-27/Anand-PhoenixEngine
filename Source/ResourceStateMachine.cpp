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


// ResourceStateMachine::DrawInspector() lives in ResourceStateMachineEditor.cpp
// now - kept separate so this file (needed for SendTrigger's FindState/
// FindClip lookups) can be linked into GameScript.dll without ImGui.
