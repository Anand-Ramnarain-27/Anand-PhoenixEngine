#pragma once
#include <string>
#include <memory>

class ResourceAnimation;

class AnimationController {
public:
    void play(ResourceAnimation* resource, bool loop = true);
    void stop();

    void update(float deltaTime);

    bool getTransform(const std::string& nodeName, Vector3& outPos, Quaternion& outRot) const;

    bool getMorphWeights(const std::string& nodeName,
        float* outWeights, uint32_t& outCount) const;

    float getCurrentTime()  const { return m_currentTime; }
    bool  isPlaying()       const { return m_playing; }
    float getSpeed()        const { return m_speed; }
    void  setSpeed(float s) { m_speed = s; }

private:
    static int findKeyIndex(const float* timestamps, uint32_t count, float t);

    ResourceAnimation* m_resource = nullptr;
    float              m_currentTime = 0.0f;
    float              m_speed = 1.0f;
    bool               m_loop = true;
    bool               m_playing = false;
};
