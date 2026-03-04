#pragma once
#include <string>
#include <cstdint>

using UID = uint64_t;

class ResourceBase
{
public:
    enum class Type
    {
        Unknown = 0,
        Mesh,
        Texture,
        Material,
        Scene,
        Model
    };

    ResourceBase(UID id, Type t)
        : uid(id), type(t) {
    }

    virtual ~ResourceBase() = default;

    UID uid = 0;
    Type type = Type::Unknown;

    std::string assetsFile;
    std::string libraryFile;

    uint32_t referenceCount = 0;

    void addRef() { ++referenceCount; }
    void releaseRef()
    {
        if (referenceCount > 0)
            --referenceCount;
    }

    bool isLoaded() const { return referenceCount > 0; }

    virtual bool LoadInMemory() = 0;
    virtual void UnloadFromMemory() = 0;
};