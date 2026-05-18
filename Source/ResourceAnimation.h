#pragma once
#include "ResourceCommon.h"
#include "SimpleMath.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>

using namespace DirectX::SimpleMath;

// ??????????????????????????????????????????????????????????????
// Channel: animation data for ONE node/bone.
// Positions and rotations have their own independent timestamp
// arrays because glTF may sample them at different rates.
// ??????????????????????????????????????????????????????????????
struct AnimChannel {
    // Position keyframes
    std::vector<Vector3>  positions;
    std::vector<float>    posTimestamps;   // seconds, sorted ascending

    // Rotation keyframes (unit quaternions)
    std::vector<Quaternion> rotations;
    std::vector<float>      rotTimestamps;

    // Scale keyframes (optional – only populated if glTF has scale data)
    std::vector<Vector3>  scales;
    std::vector<float>    scaleTimestamps;

    bool hasPositions() const { return !positions.empty(); }
    bool hasRotations() const { return !rotations.empty(); }
    bool hasScales()    const { return !scales.empty(); }
};

// ??????????????????????????????????????????????????????????????
// ResourceAnimation: the runtime-loaded animation asset.
// Derives from ResourceBase so ModuleResources manages its lifetime.
// ??????????????????????????????????????????????????????????????
class ResourceAnimation : public ResourceBase {
public:
    explicit ResourceAnimation(UID uid)
        : ResourceBase(uid, Type::Animation) {
    }
    ~ResourceAnimation() override { UnloadFromMemory(); }

    bool LoadInMemory()    override;
    void UnloadFromMemory() override;

    // Returns nullptr if no channel matches name
    const AnimChannel* getChannel(const std::string& name) const;

    float              getDuration() const { return m_duration; }
    const std::string& getName()    const { return m_name; }
    void               setName(const std::string& n) { m_name = n; }

    // Non-const access used by AnimationImporter during build/load
    std::unordered_map<std::string, AnimChannel>& getChannelsMutable()
    {
        return m_channels;
    }

    // FIX: const overload so Save(const ResourceAnimation&) can read channels
    const std::unordered_map<std::string, AnimChannel>& getChannels() const
    {
        return m_channels;
    }

    void recomputeDuration();

private:
    std::string m_name;
    float       m_duration = 0.0f;   // in seconds
    std::unordered_map<std::string, AnimChannel> m_channels;
};