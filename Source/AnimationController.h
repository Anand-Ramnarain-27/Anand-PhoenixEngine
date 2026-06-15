#pragma once
#include "ResourceCommon.h"

class ResourceAnimation;

class AnimationController {
public:
    AnimationController() = default;
    ~AnimationController();

    AnimationController(const AnimationController&) = delete;
    AnimationController& operator=(const AnimationController&) = delete;

    void Play(UID uid, bool loop = false);
    void Stop();

    void Update(float deltaTime);

    bool GetTransform(const char* name, Vector3& pos, Quaternion& rot) const;

    bool GetMorphWeights(const char* name, float* outWeights, uint32_t numTargets) const;

    bool hasMorphChannel(const char* name) const;

    bool isPlaying() const { return m_playing; }

    float CurrentTime = 0.f;
    bool Loop = false;
    UID Resource = 0;

private:
    ResourceAnimation* m_animation = nullptr;
    bool m_playing = false;
};
