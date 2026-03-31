#include "Globals.h"
#include "AnimationController.h"
#include "ResourceAnimation.h"
#include <algorithm>
#include <cstring>

void AnimationController::play(ResourceAnimation* resource, bool loop) {
    m_resource = resource;
    m_loop = loop;
    m_currentTime = 0.0f;
    m_playing = resource != nullptr;
}

void AnimationController::stop() {
    m_playing = false;
    m_currentTime = 0.0f;
}

void AnimationController::update(float deltaTime) {
    if (!m_playing || !m_resource) return;
    m_currentTime += deltaTime * m_speed;
    float dur = m_resource->getDuration();
    if (dur <= 0.0f) return;
    if (m_loop) {
        while (m_currentTime >= dur) m_currentTime -= dur;
        while (m_currentTime < 0.0f) m_currentTime += dur;
    }
    else {
        m_currentTime = std::clamp(m_currentTime, 0.0f, dur);
    }
}

int AnimationController::findKeyIndex(const float* ts, uint32_t count, float t) {
    if (count == 0) return 0;
    const float* it = std::upper_bound(ts, ts + count, t);
    int idx = (int)(it - ts) - 1;
    return std::clamp(idx, 0, (int)count - 2);
}

bool AnimationController::getTransform(
    const std::string& nodeName, Vector3& outPos, Quaternion& outRot) const
{
    if (!m_resource) return false;
    const auto& channels = m_resource->getChannels();
    auto it = channels.find(nodeName);
    if (it == channels.end()) return false;
    const Channel& ch = it->second;

    if (ch.numPositions == 1) {
        outPos = ch.positions[0];
    }
    else if (ch.numPositions > 1) {
        int k = findKeyIndex(ch.posTimeStamps.get(), ch.numPositions, m_currentTime);

        float lambda = (m_currentTime - ch.posTimeStamps[k]) / (ch.posTimeStamps[k + 1] - ch.posTimeStamps[k]);
        lambda = std::clamp(lambda, 0.0f, 1.0f);
 
        outPos = Vector3::Lerp(ch.positions[k], ch.positions[k + 1], lambda);
    }

    if (ch.numRotations == 1) {
        outRot = ch.rotations[0];
    }
    else if (ch.numRotations > 1) {
        int k = findKeyIndex(ch.rotTimeStamps.get(), ch.numRotations, m_currentTime);
        float lambda = (m_currentTime - ch.rotTimeStamps[k])
            / (ch.rotTimeStamps[k + 1] - ch.rotTimeStamps[k]);
        lambda = std::clamp(lambda, 0.0f, 1.0f);
        outRot = Quaternion::Lerp(ch.rotations[k], ch.rotations[k + 1], lambda);
        outRot.Normalize();
    }
    return true;
}

bool AnimationController::getMorphWeights(
    const std::string& nodeName, float* outWeights, uint32_t& outCount) const
{
    if (!m_resource) return false;
    const auto& morphChannels = m_resource->getMorphChannels();
    auto it = morphChannels.find(nodeName);
    if (it == morphChannels.end()) return false;
    const MorphChannel& mc = it->second;
    outCount = mc.numTargets;
    if (mc.numTime == 0) return false;
    if (mc.numTime == 1) {
        memcpy(outWeights, mc.weights.get(), mc.numTargets * sizeof(float));
        return true;
    }
    int k = findKeyIndex(mc.weightsTimes.get(), mc.numTime, m_currentTime);
    float lambda = (m_currentTime - mc.weightsTimes[k]) / (mc.weightsTimes[k + 1] - mc.weightsTimes[k]);
    lambda = std::clamp(lambda, 0.0f, 1.0f);
    const float* w0 = mc.weights.get() + k * mc.numTargets;
    const float* w1 = mc.weights.get() + (k + 1) * mc.numTargets;
    for (uint32_t i = 0; i < mc.numTargets; ++i)
        outWeights[i] = w0[i] + lambda * (w1[i] - w0[i]);
    return true;
}
