#include "Globals.h"
#include "StateMachineResource.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/prettywriter.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <algorithm>

using namespace rapidjson;

SMClip* StateMachineResource::findClip(const std::string& name) {
    for (auto& c : clips) if (c.name == name) return &c;
    return nullptr;
}
const SMClip* StateMachineResource::findClip(const std::string& name) const {
    for (const auto& c : clips) if (c.name == name) return &c;
    return nullptr;
}
SMState* StateMachineResource::findState(const std::string& name) {
    for (auto& s : states) if (s.name == name) return &s;
    return nullptr;
}
const SMState* StateMachineResource::findState(const std::string& name) const {
    for (const auto& s : states) if (s.name == name) return &s;
    return nullptr;
}

const SMTransition* StateMachineResource::findTransition(
    const std::string& fromState, const std::string& trigger) const
{
    for (const auto& tr : transitions)
        if (tr.source == fromState && tr.triggerName == trigger)
            return &tr;
    return nullptr;
}

std::string StateMachineResource::toJson() const {
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();

    doc.AddMember("defaultState", Value(defaultState.c_str(), a), a);

    Value clipsArr(kArrayType);
    for (const auto& c : clips) {
        Value cv(kObjectType);
        cv.AddMember("name", Value(c.name.c_str(), a), a);
        cv.AddMember("animUID", c.animUID, a);
        cv.AddMember("loop", c.loop, a);
        clipsArr.PushBack(cv, a);
    }
    doc.AddMember("clips", clipsArr, a);

    Value statesArr(kArrayType);
    for (const auto& s : states) {
        Value sv(kObjectType);
        sv.AddMember("name", Value(s.name.c_str(), a), a);
        sv.AddMember("clipName", Value(s.clipName.c_str(), a), a);
        sv.AddMember("nodeX", s.nodeX, a);
        sv.AddMember("nodeY", s.nodeY, a);
        statesArr.PushBack(sv, a);
    }
    doc.AddMember("states", statesArr, a);

    Value transArr(kArrayType);
    for (const auto& t : transitions) {
        Value tv(kObjectType);
        tv.AddMember("source", Value(t.source.c_str(), a), a);
        tv.AddMember("target", Value(t.target.c_str(), a), a);
        tv.AddMember("trigger", Value(t.triggerName.c_str(), a), a);
        tv.AddMember("interpolationMs", t.interpolationMs, a);
        transArr.PushBack(tv, a);
    }
    doc.AddMember("transitions", transArr, a);

    StringBuffer sb;
    PrettyWriter<StringBuffer> w(sb);
    doc.Accept(w);
    return sb.GetString();
}

bool StateMachineResource::fromJson(const std::string& json) {
    Document doc; doc.Parse(json.c_str());
    if (doc.HasParseError()) { LOG("SMResource: JSON parse error"); return false; }

    clips.clear(); states.clear(); transitions.clear();

    if (doc.HasMember("defaultState"))
        defaultState = doc["defaultState"].GetString();

    if (doc.HasMember("clips") && doc["clips"].IsArray())
        for (const auto& cv : doc["clips"].GetArray()) {
            SMClip c;
            c.name = cv["name"].GetString();
            c.animUID = cv["animUID"].GetUint64();
            c.loop = cv["loop"].GetBool();
            clips.push_back(std::move(c));
        }

    if (doc.HasMember("states") && doc["states"].IsArray())
        for (const auto& sv : doc["states"].GetArray()) {
            SMState s;
            s.name = sv["name"].GetString();
            s.clipName = sv["clipName"].GetString();
            s.nodeX = sv.HasMember("nodeX") ? sv["nodeX"].GetFloat() : 0.0f;
            s.nodeY = sv.HasMember("nodeY") ? sv["nodeY"].GetFloat() : 0.0f;
            states.push_back(std::move(s));
        }

    if (doc.HasMember("transitions") && doc["transitions"].IsArray())
        for (const auto& tv : doc["transitions"].GetArray()) {
            SMTransition t;
            t.source = tv["source"].GetString();
            t.target = tv["target"].GetString();
            t.triggerName = tv["trigger"].GetString();
            t.interpolationMs = tv["interpolationMs"].GetFloat();
            transitions.push_back(std::move(t));
        }
    return true;
}

bool StateMachineResource::saveToFile(const std::string& filePath) const {
    std::string json = toJson();
    return app->getFileSystem()->Save(filePath.c_str(), json.c_str(),
        (unsigned)json.size());
}

bool StateMachineResource::loadFromFile(const std::string& filePath) {
    char* buf = nullptr;
    unsigned size = app->getFileSystem()->Load(filePath.c_str(), &buf);
    if (!buf || size == 0) return false;
    std::string json(buf, size);
    delete[] buf;
    return fromJson(json);
}
