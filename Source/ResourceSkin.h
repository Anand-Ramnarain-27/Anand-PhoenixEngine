#pragma once
#include "ResourceCommon.h"
#include "Globals.h"
#include <vector>
#include <string>

class ResourceSkin : public ResourceBase {
public:
    explicit ResourceSkin(UID id) : ResourceBase(id, Type::Animation) {}
    ~ResourceSkin() override { UnloadFromMemory(); }

    bool LoadInMemory()     override { return true; }
    void UnloadFromMemory() override {
        jointNames.clear(); inverseBindMatrices.clear();
    }

    std::vector<std::string> jointNames;      
    std::vector<Matrix>      inverseBindMatrices; 
};
