#pragma once
#include "ResourceCommon.h"
#include "Globals.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>

struct AnimChannel {
    std::unique_ptr<Vector3[]>    positions;
    std::unique_ptr<float[]>      posTimeStamps;
    uint32_t                      numPositions = 0;

    std::unique_ptr<Quaternion[]> rotations;
    std::unique_ptr<float[]>      rotTimeStamps;
    uint32_t                      numRotations = 0;
};

struct MorphChannel {
    std::unique_ptr<float[]> times;  
    std::unique_ptr<float[]> weights;  
    uint32_t numKeyframes = 0;
    uint32_t numTargets = 0;
};

class ResourceAnimation : public ResourceBase {
public:
    explicit ResourceAnimation(UID id)
        : ResourceBase(id, Type::Animation) {
    }
    ~ResourceAnimation() override { UnloadFromMemory(); }

    bool LoadInMemory()   override; 
    void UnloadFromMemory() override;

    std::unordered_map<std::string, AnimChannel>  channels;
    std::unordered_map<std::string, MorphChannel> morphChannels; 
    float duration = 0.f;  
};
