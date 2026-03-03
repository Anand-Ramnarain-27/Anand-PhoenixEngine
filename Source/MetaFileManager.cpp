#include "Globals.h"
#include "MetaFileManager.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "3rdParty/rapidjson/document.h"
#include "3rdParty/rapidjson/prettywriter.h"
#include "3rdParty/rapidjson/stringbuffer.h"
#include <filesystem>
#include <random>
#include <chrono>

using namespace rapidjson;
namespace fs = std::filesystem;

std::string MetaFileManager::getMetaPath(const std::string& assetPath)
{
    return assetPath + ".meta";
}

bool MetaFileManager::save(const std::string& assetPath, const MetaData& meta)
{
    Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
    doc.AddMember("uid", meta.uid, a);
    doc.AddMember("type", (int)meta.type, a);
    doc.AddMember("lastModified", meta.lastModified, a);

    StringBuffer sb;
    PrettyWriter<StringBuffer> writer(sb);
    doc.Accept(writer);

    std::string metaPath = getMetaPath(assetPath);
    return app->getFileSystem()->Save(metaPath.c_str(), sb.GetString(), (unsigned)sb.GetSize());
}

bool MetaFileManager::load(const std::string& assetPath, MetaData& outMeta)
{
    std::string metaPath = getMetaPath(assetPath);
    char* buf = nullptr;
    unsigned size = app->getFileSystem()->Load(metaPath.c_str(), &buf);
    if (!buf || size == 0) return false;

    Document doc; doc.Parse(buf, size); delete[] buf;
    if (doc.HasParseError()) return false;

    if (doc.HasMember("uid"))          outMeta.uid = doc["uid"].GetUint64();
    if (doc.HasMember("type"))         outMeta.type = (ResourceBase::Type)doc["type"].GetInt();
    if (doc.HasMember("lastModified")) outMeta.lastModified = doc["lastModified"].GetUint64();
    return true;
}

bool MetaFileManager::exists(const std::string& assetPath)
{
    return app->getFileSystem()->Exists(getMetaPath(assetPath).c_str());
}

UID MetaFileManager::getOrCreateUID(const std::string& assetPath, ResourceBase::Type type)
{
    MetaData meta;
    if (load(assetPath, meta))
    {
        uint64_t currentMod = getLastModified(assetPath);
        if (currentMod != meta.lastModified) { meta.lastModified = currentMod; save(assetPath, meta); }
        return meta.uid;
    }

    meta.uid = generateUID();
    meta.type = type;
    meta.lastModified = getLastModified(assetPath);
    save(assetPath, meta);
    LOG("MetaFileManager: Created .meta for %s (uid=%llu)", assetPath.c_str(), meta.uid);
    return meta.uid;
}

UID MetaFileManager::generateUID()
{
    static std::mt19937_64 gen(std::random_device{}());
    static std::uniform_int_distribution<uint64_t> dis(1, UINT64_MAX);
    return dis(gen);
}

uint64_t MetaFileManager::getLastModified(const std::string& filePath)
{
    try {
        auto ftime = fs::last_write_time(filePath);
        auto duration = ftime.time_since_epoch();
        return (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    }
    catch (...) { return 0; }
}