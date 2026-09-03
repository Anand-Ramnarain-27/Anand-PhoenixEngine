#include "Globals.h"
#include "BuildSettings.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/prettywriter.h"
#include "3rdParty/rapidjson/stringbuffer.h"

using namespace rapidjson;

bool BuildSettings::Load(const std::string& filePath){
    auto* fs = app->getFileSystem();
    if (!fs->Exists(filePath.c_str())) return false;

    char* buf = nullptr;
    unsigned size = fs->Load(filePath.c_str(), &buf);
    if (!buf || size == 0) return false;

    Document doc;
    doc.Parse(buf, size);
    delete[] buf;

    if (doc.HasParseError() || !doc.IsObject()) return false;

    scenes.clear();
    if (doc.HasMember("Scenes") && doc["Scenes"].IsArray()){
        for (const auto& entry : doc["Scenes"].GetArray()){
            if (!entry.IsObject() || !entry.HasMember("Path") || !entry["Path"].IsString()) continue;
            BuildSceneEntry e;
            e.path = entry["Path"].GetString();
            e.enabled = entry.HasMember("Enabled") && entry["Enabled"].IsBool() ? entry["Enabled"].GetBool() : true;
            scenes.push_back(std::move(e));
        }
    }
    if (doc.HasMember("OutputDir") && doc["OutputDir"].IsString()) outputDir = doc["OutputDir"].GetString();
    if (doc.HasMember("Configuration") && doc["Configuration"].IsString()) configuration = doc["Configuration"].GetString();
    if (doc.HasMember("Platform") && doc["Platform"].IsString()) platform = doc["Platform"].GetString();
    if (doc.HasMember("ProductName") && doc["ProductName"].IsString()) productName = doc["ProductName"].GetString();
    if (doc.HasMember("StripSourceAssets") && doc["StripSourceAssets"].IsBool()) stripSourceAssets = doc["StripSourceAssets"].GetBool();

    return true;
}

bool BuildSettings::Save(const std::string& filePath) const{
    try {
        Document doc; doc.SetObject(); auto& a = doc.GetAllocator();

        Value sceneArray(kArrayType);
        for (const auto& e : scenes){
            Value entry(kObjectType);
            entry.AddMember("Path", Value(e.path.c_str(), a), a);
            entry.AddMember("Enabled", e.enabled, a);
            sceneArray.PushBack(entry, a);
        }
        doc.AddMember("Scenes", sceneArray, a);
        doc.AddMember("OutputDir", Value(outputDir.c_str(), a), a);
        doc.AddMember("Configuration", Value(configuration.c_str(), a), a);
        doc.AddMember("Platform", Value(platform.c_str(), a), a);
        doc.AddMember("ProductName", Value(productName.c_str(), a), a);
        doc.AddMember("StripSourceAssets", stripSourceAssets, a);

        StringBuffer sb;
        PrettyWriter<StringBuffer> writer(sb);
        doc.Accept(writer);
        return app->getFileSystem()->Save(filePath.c_str(), sb.GetString(), (unsigned)sb.GetSize());
    }
    catch (const std::exception& e){ LOG("BuildSettings: Save exception: %s", e.what()); return false; }
    catch (...){ LOG("BuildSettings: Unknown save exception"); return false; }
}

int BuildSettings::getBuildIndex(const std::string& scenePath) const{
    int idx = 0;
    for (const auto& e : scenes){
        if (!e.enabled) continue;
        if (e.path == scenePath) return idx;
        ++idx;
    }
    return -1;
}

std::string BuildSettings::getScenePathAtBuildIndex(int index) const{
    if (index < 0) return "";
    int idx = 0;
    for (const auto& e : scenes){
        if (!e.enabled) continue;
        if (idx == index) return e.path;
        ++idx;
    }
    return "";
}

int BuildSettings::getEnabledSceneCount() const{
    int count = 0;
    for (const auto& e : scenes) if (e.enabled) ++count;
    return count;
}
