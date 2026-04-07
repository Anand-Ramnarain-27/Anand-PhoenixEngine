#include "Globals.h"
#include "AnimationImporter.h"
#include "ResourceAnimation.h"
#include "ModuleFileSystem.h"
#include "tiny_gltf.h"
#include <vector>
#include <cstring>

static std::unique_ptr<float[]> readFloats(
    const tinygltf::Model& model, int accessorIdx, uint32_t& outCount) {
    if (accessorIdx < 0) { outCount = 0; return nullptr; }
    const auto& acc = model.accessors[accessorIdx];
    const auto& view = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[view.buffer];
    const unsigned char* data = buf.data.data() + view.byteOffset + acc.byteOffset;
    uint32_t components = 1;
    if (acc.type == TINYGLTF_TYPE_VEC3) components = 3;
    if (acc.type == TINYGLTF_TYPE_VEC4) components = 4;
    outCount = (uint32_t)acc.count;
    size_t stride = acc.ByteStride(view);
    if (!stride) stride = components * sizeof(float);
    auto arr = std::make_unique<float[]>(acc.count * components);
    for (size_t i = 0; i < acc.count; ++i)
        memcpy(arr.get() + i * components,
            data + i * stride,
            components * sizeof(float));
    return arr;
}

std::unique_ptr<ResourceAnimation> AnimationImporter::Import(
    const tinygltf::Model& model, int animIndex, const std::string& clipName)
{
    if (animIndex < 0 || animIndex >= (int)model.animations.size()) return nullptr;
    const auto& anim = model.animations[animIndex];

    std::unordered_map<std::string, Channel>      channels;
    std::unordered_map<std::string, MorphChannel> morphChannels;

    for (const auto& ch : anim.channels) {
        const auto& sampler = anim.samplers[ch.sampler];
        uint32_t timeCount = 0;
        auto     times = readFloats(model, sampler.input, timeCount);

        const std::string& nodeName =
            model.nodes[ch.target_node].name;

        if (ch.target_path == "translation") {
            uint32_t posCount = 0;
            auto     posData = readFloats(model, sampler.output, posCount);
            Channel& c = channels[nodeName];
            c.numPositions = posCount;
            c.posTimeStamps = std::move(times);
            c.positions = std::make_unique<Vector3[]>(posCount);
            for (uint32_t i = 0; i < posCount; ++i)
                c.positions[i] = { posData[i * 3], posData[i * 3 + 1], posData[i * 3 + 2] };
        }
        else if (ch.target_path == "rotation") {
            uint32_t rotCount = 0;
            auto     rotData = readFloats(model, sampler.output, rotCount);
            Channel& c = channels[nodeName];
            c.numRotations = rotCount;
            c.rotTimeStamps = std::move(times);
            c.rotations = std::make_unique<Quaternion[]>(rotCount);

            for (uint32_t i = 0; i < rotCount; ++i)
                c.rotations[i] = { rotData[i * 4], rotData[i * 4 + 1],
                                   rotData[i * 4 + 2], rotData[i * 4 + 3] };
        }
        else if (ch.target_path == "weights") {
            uint32_t wCount = 0;
            auto     wData = readFloats(model, sampler.output, wCount);
            uint32_t numTargets = (timeCount > 0) ? wCount / timeCount : 0;
            MorphChannel& mc = morphChannels[nodeName];
            mc.numTime = timeCount;
            mc.numTargets = numTargets;
            mc.weightsTimes = std::move(times);
            mc.weights = std::make_unique<float[]>(wCount);
            memcpy(mc.weights.get(), wData.get(), wCount * sizeof(float));
        }
    }

    auto res = std::make_unique<ResourceAnimation>(1);
    for (auto& [name, ch] : channels)      res->addChannel(name, std::move(ch));
    for (auto& [name, mc] : morphChannels) res->addMorphChannel(name, std::move(mc));
    res->computeDuration();
    LOG("AnimationImporter: Loaded clip '%s' duration=%.2fs",
        clipName.c_str(), res->getDuration());
    return res;
}

// Binary layout:
// [uint32 magic=0x414E494D] [float duration]
// [uint32 channelCount]
//   per channel: [uint32 nameLen][char* name][uint32 numPos][float* posTimes][float* positions (x3)]
//                [uint32 numRot][float* rotTimes][float* rotations (x4)]
// [uint32 morphCount]
//   per morph:   [uint32 nameLen][char* name][uint32 numTime][uint32 numTargets]
//                [float* weightsTimes][float* weights (numTime*numTargets)]

static constexpr uint32_t kAnimMagic = 0x414E494D;

bool AnimationImporter::Save(const ResourceAnimation& anim, const std::string& path, ModuleFileSystem* fsys)
{
    std::vector<char> buf;
    auto write = [&](const void* data, size_t sz) {
        const char* p = reinterpret_cast<const char*>(data);
        buf.insert(buf.end(), p, p + sz);
        };
    auto writeU32 = [&](uint32_t v) { write(&v, 4); };
    auto writeF32 = [&](float v) { write(&v, 4); };
    auto writeStr = [&](const std::string& s) {
        uint32_t len = (uint32_t)s.size();
        writeU32(len);
        buf.insert(buf.end(), s.begin(), s.end());
        };

    writeU32(kAnimMagic);
    float dur = anim.getDuration();
    writeF32(dur);

    const auto& channels = anim.getChannels();
    writeU32((uint32_t)channels.size());
    for (const auto& [name, ch] : channels) {
        writeStr(name);
        writeU32(ch.numPositions);
        if (ch.numPositions > 0) {
            write(ch.posTimeStamps.get(), ch.numPositions * sizeof(float));
            write(ch.positions.get(), ch.numPositions * sizeof(float) * 3);
        }
        writeU32(ch.numRotations);
        if (ch.numRotations > 0) {
            write(ch.rotTimeStamps.get(), ch.numRotations * sizeof(float));
            write(ch.rotations.get(), ch.numRotations * sizeof(float) * 4);
        }
    }

    const auto& morphs = anim.getMorphChannels();
    writeU32((uint32_t)morphs.size());
    for (const auto& [name, mc] : morphs) {
        writeStr(name);
        writeU32(mc.numTime);
        writeU32(mc.numTargets);
        if (mc.numTime > 0) {
            write(mc.weightsTimes.get(), mc.numTime * sizeof(float));
            write(mc.weights.get(), mc.numTime * mc.numTargets * sizeof(float));
        }
    }

    return fsys->Save(path.c_str(), buf.data(), (unsigned int)buf.size());
}

bool AnimationImporter::Load(const std::string& path, ResourceAnimation& outAnim, ModuleFileSystem* fsys)
{
    char* raw = nullptr;
    uint32_t size = fsys->Load(path.c_str(), &raw);
    if (!raw || size < 8) { delete[] raw; return false; }

    const char* p = raw;
    const char* end = raw + size;
    auto readU32 = [&]() -> uint32_t { uint32_t v; memcpy(&v, p, 4); p += 4; return v; };
    auto readF32 = [&]() -> float { float v;    memcpy(&v, p, 4); p += 4; return v; };
    auto readStr = [&]() -> std::string {
        uint32_t len = readU32();
        std::string s(p, len); p += len; return s;
        };

    if (readU32() != kAnimMagic) { delete[] raw; return false; }
    readF32(); 

    uint32_t channelCount = readU32();
    for (uint32_t i = 0; i < channelCount && p < end; ++i) {
        std::string name = readStr();
        Channel ch;
        ch.numPositions = readU32();
        if (ch.numPositions > 0) {
            ch.posTimeStamps = std::make_unique<float[]>(ch.numPositions);
            memcpy(ch.posTimeStamps.get(), p, ch.numPositions * sizeof(float));
            p += ch.numPositions * sizeof(float);
            ch.positions = std::make_unique<Vector3[]>(ch.numPositions);
            memcpy(ch.positions.get(), p, ch.numPositions * sizeof(float) * 3);
            p += ch.numPositions * sizeof(float) * 3;
        }
        ch.numRotations = readU32();
        if (ch.numRotations > 0) {
            ch.rotTimeStamps = std::make_unique<float[]>(ch.numRotations);
            memcpy(ch.rotTimeStamps.get(), p, ch.numRotations * sizeof(float));
            p += ch.numRotations * sizeof(float);
            ch.rotations = std::make_unique<Quaternion[]>(ch.numRotations);
            memcpy(ch.rotations.get(), p, ch.numRotations * sizeof(float) * 4);
            p += ch.numRotations * sizeof(float) * 4;
        }
        outAnim.addChannel(name, std::move(ch));
    }

    uint32_t morphCount = readU32();
    for (uint32_t i = 0; i < morphCount && p < end; ++i) {
        std::string name = readStr();
        MorphChannel mc;
        mc.numTime = readU32();
        mc.numTargets = readU32();
        if (mc.numTime > 0) {
            mc.weightsTimes = std::make_unique<float[]>(mc.numTime);
            memcpy(mc.weightsTimes.get(), p, mc.numTime * sizeof(float));
            p += mc.numTime * sizeof(float);
            uint32_t wCount = mc.numTime * mc.numTargets;
            mc.weights = std::make_unique<float[]>(wCount);
            memcpy(mc.weights.get(), p, wCount * sizeof(float));
            p += wCount * sizeof(float);
        }
        outAnim.addMorphChannel(name, std::move(mc));
    }

    delete[] raw;
    outAnim.computeDuration();
    return true;
}
