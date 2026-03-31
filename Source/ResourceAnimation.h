#pragma once
#include "ResourceCommon.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

struct Channel {
    std::unique_ptr<Vector3[]>    positions;    
    std::unique_ptr<float[]>      posTimeStamps; 
    std::unique_ptr<Quaternion[]> rotations;    
    std::unique_ptr<float[]>      rotTimeStamps;
    uint32_t numPositions = 0;
    uint32_t numRotations = 0;
};

struct MorphChannel {
    std::unique_ptr<float[]> weightsTimes;  
    std::unique_ptr<float[]> weights;    
    uint32_t numTime = 0;
    uint32_t numTargets = 0;
};

class ResourceAnimation : public ResourceBase {
public:
    explicit ResourceAnimation(UID uid);
    ~ResourceAnimation() override;

    bool LoadInMemory() override;
    void UnloadFromMemory() override;

    float getDuration() const { return m_duration; }

    const std::unordered_map<std::string, Channel>& getChannels() const
    {
        return m_channels;
    }

    const std::unordered_map<std::string, MorphChannel>& getMorphChannels() const
    {
        return m_morphChannels;
    }

    void addChannel(const std::string& nodeName, Channel ch);
    void addMorphChannel(const std::string& nodeName, MorphChannel mc);
    void computeDuration();

private:
    std::unordered_map<std::string, Channel>      m_channels;
    std::unordered_map<std::string, MorphChannel> m_morphChannels;
    float m_duration = 0.0f;
};
