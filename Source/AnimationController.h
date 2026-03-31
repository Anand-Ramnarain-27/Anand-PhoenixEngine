#pragma once
#include "ResourceCommon.h"
#include <string>
#include <memory>

class ResourceAnimation;

struct AnimNode {
    ResourceAnimation* resource = nullptr;
    float              currentTimeMs = 0.0f;   
    float              fadeTimeMs = 0.0f; 
    float              transitionMs = 0.0f;   
    float              speed = 1.0f;   
    bool               loop = true;
    AnimNode* next = nullptr; 
    ~AnimNode() { delete next; }    
};

class AnimationController {
public:
    AnimationController() = default;
    ~AnimationController() { delete m_head; }

    void play(ResourceAnimation* resource, bool loop = true, float transitionMs = 0.0f, float speed = 1.0f);
    void stop();

    void update(float deltaTimeMs);

    bool getTransform(const std::string& nodeName, Vector3& outPos, Quaternion& outRot) const;

    bool getMorphWeights(const std::string& nodeName, float* outWeights, uint32_t& outCount) const;

    bool  isPlaying()         const { return m_head != nullptr; }
    float getCurrentTimeMs()  const { return m_head ? m_head->currentTimeMs : 0.0f; }
    float getSpeed()          const { return m_head ? m_head->speed : 1.0f; }
    void  setSpeed(float s) { if (m_head) m_head->speed = s; }

private:
    bool getTransformRecursive(const AnimNode* node, const std::string& name, Vector3& outPos, Quaternion& outRot) const;

    static int findKeyIndex(const float* ts, uint32_t count, float t);
    static bool sampleChannel(const ResourceAnimation* res, float timeMs, const std::string& name, Vector3& outPos, Quaternion& outRot);

    AnimNode* m_head = nullptr;
};
