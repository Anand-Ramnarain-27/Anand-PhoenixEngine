#include "Globals.h"
#include "AnimController.h"
#include "ResourceAnimation.h"
#include <algorithm>

void AnimController::Play(ResourceAnimation* anim, bool loop) {
    m_resource = anim;
    m_timeSec = 0.f;
    m_loop = loop;
    m_playing = (anim != nullptr);
}
void AnimController::Stop() {
    m_playing = false;
    m_timeSec = 0.f;
}
float AnimController::getDuration() const {
    return m_resource ? m_resource->duration : 0.f;
}

void AnimController::Update(float dtSec) {
    if (!m_playing || !m_resource || m_resource->duration <= 0.f) return;
    m_timeSec += dtSec;
    if (m_loop) {
        // Wrap: fmod keeps us in [0, duration)
        m_timeSec = fmodf(m_timeSec, m_resource->duration);
    }
    else {
        if (m_timeSec >= m_resource->duration) {
            m_timeSec = m_resource->duration;
            m_playing = false;
        }
    }
}

// ?? Binary search helper ?????????????????????????????????????????????????
// Returns the index of the LOWER bracket for 't' in a sorted float array.
// Handles edge cases: single key, time before first key, time after last key.
static uint32_t findLower(const float* stamps, uint32_t count, float t) {
    if (count == 1) return 0;
    // std::upper_bound finds first stamp > t.
    const float* it = std::upper_bound(stamps, stamps + count, t);
    if (it == stamps)         return 0;            // t before first key
    if (it == stamps + count) return count - 1;   // t after last key
    return (uint32_t)(it - stamps) - 1;            // normal case
}

// ?? Lerp scalar ??????????????????????????????????????????????????????????
static float lerpF(float a, float b, float t) { return a + (b - a) * t; }

bool AnimController::GetTransform(const std::string& name,
    Vector3& outPos,
    Quaternion& outRot) const {
    if (!m_resource) return false;
    auto it = m_resource->channels.find(name);
    if (it == m_resource->channels.end()) return false;
    const AnimChannel& c = it->second;
    bool found = false;

    // ?? position interpolation ??
    if (c.numPositions > 0) {
        uint32_t lo = findLower(c.posTimeStamps.get(), c.numPositions, m_timeSec);
        uint32_t hi = std::min(lo + 1, c.numPositions - 1);
        if (lo == hi) {
            outPos = c.positions[lo];
        }
        else {
            float t0 = c.posTimeStamps[lo], t1 = c.posTimeStamps[hi];
            float lambda = (t1 > t0) ? (m_timeSec - t0) / (t1 - t0) : 0.f;
            outPos = Vector3::Lerp(c.positions[lo], c.positions[hi], lambda);
        }
        found = true;
    }

    // ?? rotation interpolation ??
    if (c.numRotations > 0) {
        uint32_t lo = findLower(c.rotTimeStamps.get(), c.numRotations, m_timeSec);
        uint32_t hi = std::min(lo + 1, c.numRotations - 1);
        if (lo == hi) {
            outRot = c.rotations[lo];
        }
        else {
            float t0 = c.rotTimeStamps[lo], t1 = c.rotTimeStamps[hi];
            float lambda = (t1 > t0) ? (m_timeSec - t0) / (t1 - t0) : 0.f;
            outRot = Quaternion::Slerp(c.rotations[lo], c.rotations[hi], lambda);
        }
        found = true;
    }
    return found;
}

bool AnimController::GetMorphWeights(const std::string& name,
    float* out, uint32_t count) const {
    if (!m_resource || count == 0) return false;
    auto it = m_resource->morphChannels.find(name);
    if (it == m_resource->morphChannels.end()) return false;
    const MorphChannel& mc = it->second;
    if (mc.numKeyframes == 0 || mc.numTargets == 0) return false;
    uint32_t n = std::min(count, mc.numTargets);

    uint32_t lo = findLower(mc.times.get(), mc.numKeyframes, m_timeSec);
    uint32_t hi = std::min(lo + 1, mc.numKeyframes - 1);
    float lambda = 0.f;
    if (lo != hi) {
        float t0 = mc.times[lo], t1 = mc.times[hi];
        lambda = (t1 > t0) ? (m_timeSec - t0) / (t1 - t0) : 0.f;
    }
    const float* w0 = mc.weights.get() + (size_t)lo * mc.numTargets;
    const float* w1 = mc.weights.get() + (size_t)hi * mc.numTargets;
    for (uint32_t i = 0; i < n; ++i)
        out[i] = lerpF(w0[i], w1[i], lambda);
    return true;
}
