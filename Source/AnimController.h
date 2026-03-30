#pragma once
#include "Globals.h"
#include <string>
class ResourceAnimation;

class AnimController {
public:
    AnimController() = default;

    void Play(ResourceAnimation* anim, bool loop = true);
    void Stop();
    void Update(float deltaSec);   // call with dt in SECONDS

    // Returns false if the channel doesn't exist in the animation.
    bool GetTransform(const std::string& nodeName,
        Vector3& outPos, Quaternion& outRot) const;

    // Phase 3 — fills outWeights[0..count-1] and returns false if no morph channel.
    bool GetMorphWeights(const std::string& nodeName,
        float* outWeights, uint32_t count) const;

    bool isPlaying()  const { return m_playing; }
    float getTime()   const { return m_timeSec; }
    float getDuration()const;

private:
    ResourceAnimation* m_resource = nullptr;
    float m_timeSec = 0.f;
    bool  m_loop = true;
    bool  m_playing = false;
};
