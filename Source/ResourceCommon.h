#pragma once
#include <string>
#include <cstdint>

using UID = uint64_t;

class ResourceBase {
public:
    enum class Type { Unknown = 0, Mesh, Texture, Material, Scene, Model, Animation };

    ResourceBase(UID id, Type t) : uid(id), type(t) {}
    virtual ~ResourceBase() = default;

    UID uid = 0;
    Type type = Type::Unknown;
    std::string assetsFile;
    std::string libraryFile;
    uint32_t referenceCount = 0;

    void addRef() { ++referenceCount; }
    void releaseRef() { if (referenceCount > 0) --referenceCount; }
    bool isLoaded() const { return referenceCount > 0; }

    virtual bool LoadInMemory() = 0;
    virtual void UnloadFromMemory() = 0;
};

struct MetaData {
    UID uid = 0;
    uint64_t lastModified = 0;
    ResourceBase::Type type = ResourceBase::Type::Unknown;
};

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