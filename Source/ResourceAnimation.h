#pragma once
#include "ResourceCommon.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <cstdint>

class ResourceAnimation : public ResourceBase {
public:
    struct Channel {
        std::unique_ptr<Vector3[]> positions;
        std::unique_ptr<float[]> posTimeStamps;
        uint32_t posCount = 0;

        std::unique_ptr<Quaternion[]> rotations;
        std::unique_ptr<float[]> rotTimeStamps;
        uint32_t rotCount = 0;
    };

    // One morph-weight channel per target node.
    // weights is a flat array: [t0_w0, t0_w1, ..., t0_wN, t1_w0, ...]
    // Access: weights[keyframe * numTargets + targetIndex]
    struct MorphChannel {
        std::unique_ptr<float[]> weightsTimes; // keyframe timestamps, size = numTime
        std::unique_ptr<float[]> weights; // flat weight array, size = numTime * numTargets
        uint32_t numTime = 0;
        uint32_t numTargets = 0;
    };

    explicit ResourceAnimation(UID uid);

    bool LoadInMemory() override;
    void UnloadFromMemory() override;

    const std::string& getAnimName() const { return m_name; }
    float getDuration() const { return m_duration; }
    const std::unordered_map<std::string, Channel>& getChannels() const { return m_channels; }
    const std::unordered_map<std::string, MorphChannel>& getMorphChannels() const { return m_morphChannels; }

    // Returns nullptr if no morph channel exists for nodeName.
    const MorphChannel* getMorphChannel(const std::string& nodeName) const;

private:
    std::string m_name;
    float m_duration = 0.f;
    std::unordered_map<std::string, Channel> m_channels;
    std::unordered_map<std::string, MorphChannel> m_morphChannels;
};
