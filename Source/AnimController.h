#pragma once
#include "ResourceAnimation.h"
#include "ResourceCommon.h"

class AnimController {
public:
    void play(UID animUID, bool loop = true);
    void stop();
    void update(float dt);  // dt in seconds

    bool isPlaying() const { return m_playing; }
    float getCurrentTime() const { return m_currentTime; }
    UID   getAnimUID()    const { return m_animUID; }

    /// Fill outPos and outRot with interpolated transform for
    /// the channel matching 'channelName' at current time.
    /// Returns false if no channel found (caller keeps default).
    bool getTransform(const std::string& channelName,
        Vector3& outPos,
        Quaternion& outRot,
        Vector3& outScale) const;

private:
    static float interpolateLambda(
        const std::vector<float>& timestamps,
        float t, int& outLow, int& outHigh);

    ResourceAnimation* getResource() const;

    UID   m_animUID = 0;
    float m_currentTime = 0.0f;
    bool  m_playing = false;
    bool  m_loop = true;
};