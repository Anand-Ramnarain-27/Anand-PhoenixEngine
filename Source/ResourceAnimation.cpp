#include "Globals.h"
#include "ResourceAnimation.h"
#include "ImporterUtils.h"
#include <cstring>

namespace {
struct AnimFileHeader {
    uint32_t magic = 0x414E494D;
    uint32_t version = 2;
    uint32_t animNameLen = 0;
    uint32_t channelCount = 0;
    float duration = 0.f;
};
}

ResourceAnimation::ResourceAnimation(UID uid) : ResourceBase(uid, Type::Animation){}

const ResourceAnimation::MorphChannel* ResourceAnimation::getMorphChannel(const std::string& nodeName) const{
    auto it = m_morphChannels.find(nodeName);
    return (it != m_morphChannels.end()) ? &it->second : nullptr;
}

bool ResourceAnimation::LoadInMemory(){
    m_channels.clear();
    m_morphChannels.clear();
    m_name.clear();
    m_duration = 0.f;

    AnimFileHeader header;
    std::vector<char> raw;
    if (!ImporterUtils::LoadBuffer(libraryFile, header, raw)) return false;
    if (!ImporterUtils::ValidateHeader(header, 0x414E494D)) return false;

    const char* cur = raw.data() + sizeof(AnimFileHeader);
    const char* end = raw.data() + raw.size();

    if (cur + header.animNameLen > end) return false;
    m_name.assign(cur, header.animNameLen);
    cur += header.animNameLen;
    m_duration = header.duration;

    m_channels.reserve(header.channelCount);

    for (uint32_t i = 0; i < header.channelCount; ++i){
        if (cur + 3 * sizeof(uint32_t) > end) return false;

        uint32_t nameLen, posCount, rotCount;
        memcpy(&nameLen, cur, sizeof(uint32_t));
        memcpy(&posCount, cur + sizeof(uint32_t), sizeof(uint32_t));
        memcpy(&rotCount, cur + 2 * sizeof(uint32_t), sizeof(uint32_t));
        cur += 3 * sizeof(uint32_t);

        if (cur + nameLen > end) return false;
        std::string nodeName(cur, nameLen);
        cur += nameLen;

        Channel ch;

        if (posCount > 0){
            size_t timeBytes = posCount * sizeof(float);
            size_t posBytes = posCount * sizeof(Vector3);
            if (cur + timeBytes + posBytes > end) return false;

            ch.posCount = posCount;
            ch.posTimeStamps = std::make_unique<float[]>(posCount);
            ch.positions = std::make_unique<Vector3[]>(posCount);
            memcpy(ch.posTimeStamps.get(), cur, timeBytes);
            memcpy(ch.positions.get(), cur + timeBytes, posBytes);
            cur += timeBytes + posBytes;
        }

        if (rotCount > 0){
            size_t timeBytes = rotCount * sizeof(float);
            size_t rotBytes = rotCount * sizeof(Quaternion);
            if (cur + timeBytes + rotBytes > end) return false;

            ch.rotCount = rotCount;
            ch.rotTimeStamps = std::make_unique<float[]>(rotCount);
            ch.rotations = std::make_unique<Quaternion[]>(rotCount);
            memcpy(ch.rotTimeStamps.get(), cur, timeBytes);
            memcpy(ch.rotations.get(), cur + timeBytes, rotBytes);
            cur += timeBytes + rotBytes;
        }

        m_channels.emplace(std::move(nodeName), std::move(ch));
    }

    if (header.version >= 2 && cur + sizeof(uint32_t) <= end){
        uint32_t morphCount = 0;
        memcpy(&morphCount, cur, sizeof(uint32_t));
        cur += sizeof(uint32_t);

        m_morphChannels.reserve(morphCount);

        for (uint32_t i = 0; i < morphCount; ++i){
            if (cur + 3 * sizeof(uint32_t) > end) break;

            uint32_t nameLen, numTime, numTargets;
            memcpy(&nameLen, cur, sizeof(uint32_t));
            memcpy(&numTime, cur + sizeof(uint32_t), sizeof(uint32_t));
            memcpy(&numTargets, cur + 2 * sizeof(uint32_t), sizeof(uint32_t));
            cur += 3 * sizeof(uint32_t);

            if (cur + nameLen > end) break;
            std::string nodeName(cur, nameLen);
            cur += nameLen;

            if (numTime == 0 || numTargets == 0) continue;

            size_t timeBytes = numTime * sizeof(float);
            size_t weightBytes = numTime * numTargets * sizeof(float);
            if (cur + timeBytes + weightBytes > end) break;

            MorphChannel mc;
            mc.numTime = numTime;
            mc.numTargets = numTargets;
            mc.weightsTimes = std::make_unique<float[]>(numTime);
            mc.weights = std::make_unique<float[]>(numTime * numTargets);
            memcpy(mc.weightsTimes.get(), cur, timeBytes);
            memcpy(mc.weights.get(), cur + timeBytes, weightBytes);
            cur += timeBytes + weightBytes;

            m_morphChannels.emplace(std::move(nodeName), std::move(mc));
        }
    }

    if (!m_morphChannels.empty()){
        std::string nodeList;
        for (const auto& [name, mc] : m_morphChannels){
            if (!nodeList.empty()) nodeList += ", ";
            nodeList += "'" + name + "'(" + std::to_string(mc.numTargets) + " targets/"
                      + std::to_string(mc.numTime) + " keys)";
        }
        LOG("ResourceAnimation '%s': %zu morph channel(s) — %s",
            m_name.c_str(), m_morphChannels.size(), nodeList.c_str());
    } else if (!m_channels.empty()){
        LOG("ResourceAnimation '%s': %zu transform channel(s), no morph channels",
            m_name.c_str(), m_channels.size());
    }

    return !m_channels.empty() || !m_morphChannels.empty();
}

void ResourceAnimation::UnloadFromMemory(){
    m_channels.clear();
    m_morphChannels.clear();
    m_name.clear();
    m_duration = 0.f;
}
