#pragma once
// Library/metadata/*.meta files: an asset's UID, type and last import time.

#include "ResourceCommon.h"
#include <string>
#include <cstdint>

/// Reads and writes .meta files; getOrCreateUID gives an asset a stable UID on first import.
class MetaFileManager {
public:
    static std::string getMetaPath(const std::string& assetPath);
    static bool save(const std::string& assetPath, const MetaData& meta);
    static bool load(const std::string& assetPath, MetaData& outMeta);
    static bool exists(const std::string& assetPath);
    static UID getOrCreateUID(const std::string& assetPath, ResourceBase::Type type);
    static UID generateUID();
    static uint64_t getLastModified(const std::string& filePath);
};
