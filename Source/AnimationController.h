#pragma once
#include "ResourceCommon.h"

class ResourceAnimation;

// Standalone animation playback controller.
// Call Play() to load and start an animation, Update(dt) each frame to advance time,
// and GetTransform() to sample a node's position and rotation at the current time.
class AnimationController {
public:
    AnimationController() = default;
    ~AnimationController();

    // Non-copyable (owns a ref-counted ResourceAnimation pointer).
    AnimationController(const AnimationController&) = delete;
    AnimationController& operator=(const AnimationController&) = delete;

    void Play(UID uid, bool loop = false);
    void Stop();

    // Advances CurrentTime by deltaTime, handles looping / clamping.
    void Update(float deltaTime);

    // Samples position and rotation for the node named 'name' at CurrentTime.
    // Uses std::upper_bound on the timestamp arrays and linear interpolation.
    // Returns false if the animation is not loaded or the node has no channel.
    bool GetTransform(const char* name, Vector3& pos, Quaternion& rot) const;

    // Samples morph-target weights for the node named 'name' at CurrentTime.
    // outWeights must point to a caller-allocated array of at least numTargets floats.
    // Returns false if the animation is not loaded or the node has no morph channel;
    // the output array is NOT modified in that case.
    bool GetMorphWeights(const char* name, float* outWeights, uint32_t numTargets) const;

    bool isPlaying() const { return m_playing; }

    // Public state members (as specified).
    float CurrentTime = 0.f;
    bool  Loop        = false;
    UID   Resource    = 0;

private:
    ResourceAnimation* m_animation = nullptr;
    bool               m_playing   = false;
};
