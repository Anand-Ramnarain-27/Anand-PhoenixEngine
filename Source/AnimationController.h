#pragma once
// Plays one animation clip (a ResourceAnimation) and samples its node transforms and morph weights.

#include "ResourceCommon.h"

class ResourceAnimation;

/// One clip's playback clock. ComponentAnimation owns one per layer and samples it by node name each frame.
class AnimationController {
public:
    AnimationController() = default;
    ~AnimationController();

    AnimationController(const AnimationController&) = delete;
    AnimationController& operator=(const AnimationController&) = delete;

    /// Starts `uid` from the beginning (requesting the resource, releasing the previous clip if it differs).
    void Play(UID uid, bool loop = false);
    void Stop();

    void Update(float deltaTime);

    /// The animated local transform of node `name` at the current time; false if the clip doesn't animate it.
    bool GetTransform(const char* name, Vector3& pos, Quaternion& rot) const;

    /// Fills `numTargets` morph weights for mesh node `name`; false if the clip has no weight channel for it.
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
