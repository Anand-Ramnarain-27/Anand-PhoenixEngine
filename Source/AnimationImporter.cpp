#include "Globals.h"
#include "AnimationImporter.h"
#include "ResourceAnimation.h"
#include "tiny_gltf.h"
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
