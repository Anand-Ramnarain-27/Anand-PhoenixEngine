#pragma once
#include <string>
#include <cstdint>

using UID = uint64_t;

class ResourceBase
{
public:
    enum class Type { Unknown = 0, Mesh = 1, Texture = 2, Material = 3, Scene = 4, Model = 5 };

    ResourceBase() = default;
    virtual ~ResourceBase() = default;

    UID         uid = 0;    
    Type        type = Type::Unknown;
    std::string assetsFile;        
    std::string libraryFile;  

    uint32_t referenceCount = 0;
    void addRef() { ++referenceCount; }
    void release() { if (referenceCount > 0) --referenceCount; }
    bool isLoaded() const { return referenceCount > 0; }
};