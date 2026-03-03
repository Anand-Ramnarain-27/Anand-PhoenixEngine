#pragma once
#include "ResourceBase.h"
#include <string>
#include <cstdint>

struct MetaData
{
    UID      uid = 0;
    uint64_t lastModified = 0;   
    ResourceBase::Type type = ResourceBase::Type::Unknown;
};

class MetaFileManager
{
public:
    static std::string getMetaPath(const std::string& assetPath);

    static bool save(const std::string& assetPath, const MetaData& meta);
    static bool load(const std::string& assetPath, MetaData& outMeta);
    static bool exists(const std::string& assetPath);

    static UID getOrCreateUID(const std::string& assetPath, ResourceBase::Type type);

    static UID      generateUID();
    static uint64_t getLastModified(const std::string& filePath);
};