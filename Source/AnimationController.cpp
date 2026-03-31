#include "Globals.h"
#include "AnimationController.h"
#include "ResourceAnimation.h"
#include <algorithm>
#include <cstring>

void AnimationController::play(
    ResourceAnimation* resource, bool loop,
    float transitionMs, float speed)
{
    if (transitionMs <= 0.0f) {
        delete m_head;
        m_head = nullptr;
    }

    AnimNode* node = new AnimNode();
    node->resource = resource;
    node->loop = loop;
    node->speed = speed;
    node->transitionMs = transitionMs;
    node->fadeTimeMs = 0.0f;
    node->currentTimeMs = 0.0f;
    node->next = m_head; 
    m_head = node;
}

void AnimationController::stop() {
    delete m_head;
    m_head = nullptr;
}

void AnimationController::update(float dtMs) {
    AnimNode** ptr = &m_head;
    while (*ptr) {
        AnimNode* node = *ptr;
        node->currentTimeMs += dtMs * node->speed;
        if (node->resource) {
            float durMs = node->resource->getDuration() * 1000.0f;
            if (durMs > 0.0f) {
                if (node->loop) {
                    while (node->currentTimeMs >= durMs) node->currentTimeMs -= durMs;
                }
                else {
                    node->currentTimeMs = std::min(node->currentTimeMs, durMs);
                }
            }
        }
    
        if (node->transitionMs > 0.0f) {
            node->fadeTimeMs += dtMs;
         
            if (node->fadeTimeMs >= node->transitionMs && node->next != nullptr) {
          
                delete node->next;
                node->next = nullptr;
            }
        }
        ptr = &node->next;
    }
}


bool AnimationController::sampleChannel(
    const ResourceAnimation* res, float timeMs,
    const std::string& name, Vector3& outPos, Quaternion& outRot)
{
    if (!res) return false;
    const auto& channels = res->getChannels();
    auto it = channels.find(name);
    if (it == channels.end()) return false;
    const Channel& ch = it->second;
    float timeSec = timeMs / 1000.0f;

    if (ch.numPositions == 1) {
        outPos = ch.positions[0];
    }
    else if (ch.numPositions > 1) {
        int k = findKeyIndex(ch.posTimeStamps.get(), ch.numPositions, timeSec);
        float lambda = (timeSec - ch.posTimeStamps[k])
            / (ch.posTimeStamps[k + 1] - ch.posTimeStamps[k]);
        lambda = std::clamp(lambda, 0.0f, 1.0f);
        outPos = Vector3::Lerp(ch.positions[k], ch.positions[k + 1], lambda);
    }

    if (ch.numRotations == 1) {
        outRot = ch.rotations[0];
    }
    else if (ch.numRotations > 1) {
        int k = findKeyIndex(ch.rotTimeStamps.get(), ch.numRotations, timeSec);
        float lambda = (timeSec - ch.rotTimeStamps[k])
            / (ch.rotTimeStamps[k + 1] - ch.rotTimeStamps[k]);
        lambda = std::clamp(lambda, 0.0f, 1.0f);
        outRot = Quaternion::Lerp(ch.rotations[k], ch.rotations[k + 1], lambda);
        outRot.Normalize();
    }
    return true;
}

int AnimationController::findKeyIndex(const float* ts, uint32_t count, float t) {
    if (count == 0) return 0;
    const float* it = std::upper_bound(ts, ts + count, t);
    int idx = (int)(it - ts) - 1;
    return std::clamp(idx, 0, (int)count - 2);
}

bool AnimationController::getTransformRecursive(
    const AnimNode* node, const std::string& name,
    Vector3& outPos, Quaternion& outRot) const
{
    if (!node) return false;

    Vector3    myPos = outPos;
    Quaternion myRot = outRot;
    bool haveMe = sampleChannel(node->resource, node->currentTimeMs, name, myPos, myRot);

    if (!node->next) {
        if (haveMe) { outPos = myPos; outRot = myRot; }
        return haveMe;
    }

    Vector3    fromPos = outPos;
    Quaternion fromRot = outRot;
    bool haveFrom = getTransformRecursive(node->next, name, fromPos, fromRot);

    if (!haveMe && !haveFrom) return false;
    if (!haveFrom) { outPos = myPos;   outRot = myRot;   return true; }
    if (!haveMe) { outPos = fromPos; outRot = fromRot; return true; }

    float weight = (node->transitionMs > 0.0f)
        ? std::clamp(node->fadeTimeMs / node->transitionMs, 0.0f, 1.0f)
        : 1.0f;

    outPos = Vector3::Lerp(fromPos, myPos, weight);
    outRot = Quaternion::Lerp(fromRot, myRot, weight);
    outRot.Normalize();
    return true;
}

bool AnimationController::getTransform(
    const std::string& nodeName,
    Vector3& outPos, Quaternion& outRot) const
{
    return getTransformRecursive(m_head, nodeName, outPos, outRot);
}

bool AnimationController::getMorphWeights(
    const std::string& nodeName,
    float* outWeights, uint32_t& outCount) const
{
    if (!m_head || !m_head->resource) return false;
    const auto& mchannels = m_head->resource->getMorphChannels();
    auto it = mchannels.find(nodeName);
    if (it == mchannels.end()) return false;
    const MorphChannel& mc = it->second;
    outCount = mc.numTargets;
    if (mc.numTime == 0) return false;
    float timeSec = m_head->currentTimeMs / 1000.0f;
    if (mc.numTime == 1) {
        memcpy(outWeights, mc.weights.get(), mc.numTargets * sizeof(float));
        return true;
    }
    int k = findKeyIndex(mc.weightsTimes.get(), mc.numTime, timeSec);
    float lambda = (timeSec - mc.weightsTimes[k])
        / (mc.weightsTimes[k + 1] - mc.weightsTimes[k]);
    lambda = std::clamp(lambda, 0.0f, 1.0f);
    const float* w0 = mc.weights.get() + k * mc.numTargets;
    const float* w1 = mc.weights.get() + (k + 1) * mc.numTargets;
    for (uint32_t i = 0; i < mc.numTargets; ++i)
        outWeights[i] = w0[i] + lambda * (w1[i] - w0[i]);
    return true;
}
