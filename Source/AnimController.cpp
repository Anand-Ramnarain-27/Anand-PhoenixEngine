#include "Globals.h"
#include "AnimController.h"
#include "Application.h"
#include "ModuleResources.h"
#include <algorithm>

ResourceAnimation* AnimController::getResource() const {
    if (m_animUID == 0) return nullptr;
    return app->getResources()->RequestAnimation(m_animUID);
}

void AnimController::play(UID animUID, bool loop) {
    m_animUID = animUID;
    m_currentTime = 0.0f;
    m_loop = loop;
    m_playing = true;
}

void AnimController::stop() {
    m_playing = false;
    m_currentTime = 0.0f;
}

void AnimController::update(float dt) {
    if (!m_playing) return;
    ResourceAnimation* res = getResource();
    if (!res) return;

    m_currentTime += dt;

    float dur = res->getDuration();
    if (dur <= 0.0f) { m_playing = false; return; }

    if (m_loop) {
        while (m_currentTime >= dur) m_currentTime -= dur;
    }
    else if (m_currentTime >= dur) {
        m_currentTime = dur;
        m_playing = false;
    }
}

// ?? key interpolation helper ???????????????????????????????????
// Finds the two adjacent keyframe indices for 't' in a sorted
// timestamps array. Returns lambda ? [0,1] for blending.
// outLow  = lower bound index
// outHigh = upper bound index
float AnimController::interpolateLambda(
    const std::vector<float>& timestamps,
    float  t,
    int& outLow,
    int& outHigh)
{
    const int n = (int)timestamps.size();
    if (n == 0) { outLow = outHigh = 0; return 0.0f; }
    if (n == 1) { outLow = outHigh = 0; return 0.0f; }

    // upper_bound returns iterator to first element > t
    auto it = std::upper_bound(
        timestamps.begin(), timestamps.end(), t);

    if (it == timestamps.end()) {
        // t is past the last keyframe — clamp to last
        outLow = n - 1;
        outHigh = n - 1;
        return 0.0f;
    }
    if (it == timestamps.begin()) {
        // t is before the first keyframe — clamp to first
        outLow = 0;
        outHigh = 0;
        return 0.0f;
    }

    outHigh = (int)(it - timestamps.begin());
    outLow = outHigh - 1;

    float t0 = timestamps[outLow];
    float t1 = timestamps[outHigh];
    float dt = t1 - t0;
    if (dt <= 1e-6f) return 0.0f;

    return (t - t0) / dt;  // lambda ? [0,1]
}

bool AnimController::getTransform(
    const std::string& channelName,
    Vector3& outPos,
    Quaternion& outRot,
    Vector3& outScale) const
{
    ResourceAnimation* res = getResource();
    if (!res) return false;

    const AnimChannel* ch = res->getChannel(channelName);
    if (!ch) return false;

    bool found = false;

    // ?? position ??????????????????????????????????????????????
    if (ch->hasPositions()) {
        int lo, hi;
        float lambda = interpolateLambda(
            ch->posTimestamps, m_currentTime, lo, hi);
        outPos = Vector3::Lerp(ch->positions[lo],
            ch->positions[hi],
            lambda);
        found = true;
    }

    // ?? rotation ??????????????????????????????????????????????
    if (ch->hasRotations()) {
        int lo, hi;
        float lambda = interpolateLambda(
            ch->rotTimestamps, m_currentTime, lo, hi);
        // Lerp is sufficient; Slerp is more "correct" but
        // nearly identical and 3x slower for unit quaternions
        outRot = Quaternion::Lerp(ch->rotations[lo],
            ch->rotations[hi],
            lambda);
        outRot.Normalize();
        found = true;
    }

    // ?? scale ?????????????????????????????????????????????????
    if (ch->hasScales()) {
        int lo, hi;
        float lambda = interpolateLambda(
            ch->scaleTimestamps, m_currentTime, lo, hi);
        outScale = Vector3::Lerp(ch->scales[lo],
            ch->scales[hi],
            lambda);
        found = true;
    }

    return found;
}