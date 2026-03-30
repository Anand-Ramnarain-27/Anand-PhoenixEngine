#pragma once
#include "Globals.h"
#include <string>
class ResourceAnimation;

class AnimController {
public:
    AnimController() = default;

    void Play(ResourceAnimation* anim, bool loop = true);
    void Stop();
    void Update(float deltaSec);   
     
    bool GetTransform(const std::string& nodeName, Vector3& outPos, Quaternion& outRot) const;
     
    bool GetMorphWeights(const std::string& nodeName, float* outWeights, uint32_t count) const;

    bool isPlaying()  const { return m_playing; }
    float getTime()   const { return m_timeSec; }
    float getDuration()const;

private:
    ResourceAnimation* m_resource = nullptr;
    float m_timeSec = 0.f;
    bool  m_loop = true;
    bool  m_playing = false;
};
