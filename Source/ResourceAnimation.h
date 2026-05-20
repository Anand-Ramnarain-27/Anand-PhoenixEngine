#pragma once
#include "ResourceCommon.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <cstdint>

class ResourceAnimation : public ResourceBase {
public:
    struct Channel {
        std::unique_ptr<Vector3[]>    positions;
        std::unique_ptr<float[]>      posTimeStamps;
        uint32_t                      posCount = 0;

        std::unique_ptr<Quaternion[]> rotations;
        std::unique_ptr<float[]>      rotTimeStamps;
        uint32_t                      rotCount = 0;
    };

    explicit ResourceAnimation(UID uid);

    bool LoadInMemory() override;
    void UnloadFromMemory() override;

    const std::string& getAnimName() const { return m_name; }
    float getDuration() const { return m_duration; }
    const std::unordered_map<std::string, Channel>& getChannels() const { return m_channels; }

private:
    std::string m_name;
    float m_duration = 0.f;
    std::unordered_map<std::string, Channel> m_channels;
};
