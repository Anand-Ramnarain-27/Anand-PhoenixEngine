#include "Globals.h"
#include "ResourceAnimation.h"
#include "ImporterUtils.h"
#include <cstring>

// Must match the layout written by AnimationImporter::importOne.
namespace {
struct AnimFileHeader {
    uint32_t magic       = 0x414E494D;
    uint32_t version     = 1;
    uint32_t animNameLen = 0;
    uint32_t channelCount = 0;
    float    duration    = 0.f;
};
} // namespace

ResourceAnimation::ResourceAnimation(UID uid) : ResourceBase(uid, Type::Animation) {}

bool ResourceAnimation::LoadInMemory() {
    m_channels.clear();
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

    for (uint32_t i = 0; i < header.channelCount; ++i) {
        if (cur + 3 * sizeof(uint32_t) > end) return false;

        uint32_t nameLen, posCount, rotCount;
        memcpy(&nameLen,  cur,                  sizeof(uint32_t));
        memcpy(&posCount, cur + sizeof(uint32_t),     sizeof(uint32_t));
        memcpy(&rotCount, cur + 2 * sizeof(uint32_t), sizeof(uint32_t));
        cur += 3 * sizeof(uint32_t);

        if (cur + nameLen > end) return false;
        std::string nodeName(cur, nameLen);
        cur += nameLen;

        Channel ch;

        if (posCount > 0) {
            size_t timeBytes = posCount * sizeof(float);
            size_t posBytes  = posCount * sizeof(Vector3);
            if (cur + timeBytes + posBytes > end) return false;

            ch.posCount      = posCount;
            ch.posTimeStamps = std::make_unique<float[]>(posCount);
            ch.positions     = std::make_unique<Vector3[]>(posCount);
            memcpy(ch.posTimeStamps.get(), cur,             timeBytes);
            memcpy(ch.positions.get(),     cur + timeBytes, posBytes);
            cur += timeBytes + posBytes;
        }

        if (rotCount > 0) {
            size_t timeBytes = rotCount * sizeof(float);
            size_t rotBytes  = rotCount * sizeof(Quaternion);
            if (cur + timeBytes + rotBytes > end) return false;

            ch.rotCount      = rotCount;
            ch.rotTimeStamps = std::make_unique<float[]>(rotCount);
            ch.rotations     = std::make_unique<Quaternion[]>(rotCount);
            memcpy(ch.rotTimeStamps.get(), cur,             timeBytes);
            memcpy(ch.rotations.get(),     cur + timeBytes, rotBytes);
            cur += timeBytes + rotBytes;
        }

        m_channels.emplace(std::move(nodeName), std::move(ch));
    }

    return !m_channels.empty();
}

void ResourceAnimation::UnloadFromMemory() {
    m_channels.clear();
    m_name.clear();
    m_duration = 0.f;
}
