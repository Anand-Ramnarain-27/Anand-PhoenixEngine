#include "Globals.h"
#include "AnimImporter.h"
#include "ResourceAnimation.h"
#include "ImporterUtils.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "tiny_gltf.h"
#include <algorithm>
#include <cstring>

static const unsigned char* accData(const tinygltf::Model& m,
    const tinygltf::Accessor& a) {
    const auto& v = m.bufferViews[a.bufferView];
    return m.buffers[v.buffer].data.data() + v.byteOffset + a.byteOffset;
}

static size_t accStride(const tinygltf::Model& m,
    const tinygltf::Accessor& a, size_t def) {
    size_t s = a.ByteStride(m.bufferViews[a.bufferView]);
    return s ? s : def;
}

static void writeStr(std::vector<char>& buf, const std::string& s) {
    uint32_t len = (uint32_t)s.size();
    buf.insert(buf.end(), (char*)&len, (char*)&len + 4);
    buf.insert(buf.end(), s.begin(), s.end());
}
static std::string readStr(const char*& cur) {
    uint32_t len; memcpy(&len, cur, 4); cur += 4;
    std::string s(cur, len); cur += len; return s;
}

bool AnimImporter::Import(const tinygltf::Model& model,
    int animIndex,
    const std::string& outputFile) {
    if (animIndex < 0 || animIndex >= (int)model.animations.size()) return false;
    const auto& anim = model.animations[animIndex];

    std::unordered_map<std::string, AnimChannel>  channels;
    std::unordered_map<std::string, MorphChannel> morphCh;
    float duration = 0.f;

    for (const auto& ch : anim.channels) {
        if (ch.target_node < 0 || ch.target_node >= (int)model.nodes.size()) continue;
        const std::string& nodeName = model.nodes[ch.target_node].name;
        const auto& sampler = anim.samplers[ch.sampler];

        const auto& tsAcc = model.accessors[sampler.input];
        uint32_t numKeys = (uint32_t)tsAcc.count;
        const unsigned char* tsData = accData(model, tsAcc);
        size_t tsStride = accStride(model, tsAcc, sizeof(float));

        for (uint32_t k = 0; k < numKeys; ++k) {
            float t; memcpy(&t, tsData + k * tsStride, sizeof(float));
            duration = std::max(duration, t);
        }

        const auto& outAcc = model.accessors[sampler.output];
        const unsigned char* outData = accData(model, outAcc);

        if (ch.target_path == "translation") {
            auto& c = channels[nodeName];
            c.numPositions = numKeys;
            c.positions = std::make_unique<Vector3[]>(numKeys);
            c.posTimeStamps = std::make_unique<float[]>(numKeys);
            size_t stride = accStride(model, outAcc, sizeof(float) * 3);
            for (uint32_t k = 0; k < numKeys; ++k) {
                memcpy(&c.posTimeStamps[k], tsData + k * tsStride, sizeof(float));
                float xyz[3]; memcpy(xyz, outData + k * stride, sizeof(xyz));
                c.positions[k] = { xyz[0], xyz[1], xyz[2] };
            }
        }
        else if (ch.target_path == "rotation") {
            auto& c = channels[nodeName];
            c.numRotations = numKeys;
            c.rotations = std::make_unique<Quaternion[]>(numKeys);
            c.rotTimeStamps = std::make_unique<float[]>(numKeys);
            size_t stride = accStride(model, outAcc, sizeof(float) * 4);
            for (uint32_t k = 0; k < numKeys; ++k) {
                memcpy(&c.rotTimeStamps[k], tsData + k * tsStride, sizeof(float));
                float xyzw[4]; memcpy(xyzw, outData + k * stride, sizeof(xyzw));
                c.rotations[k] = { xyzw[0],xyzw[1],xyzw[2],xyzw[3] };
            }
        }
        else if (ch.target_path == "weights") {
            uint32_t numTargets = (numKeys > 0) ? (uint32_t)(outAcc.count / numKeys) : 0;
            if (numTargets == 0) continue;
            auto& mc = morphCh[nodeName];
            mc.numKeyframes = numKeys;
            mc.numTargets = numTargets;
            mc.times = std::make_unique<float[]>(numKeys);
            mc.weights = std::make_unique<float[]>((size_t)numKeys * numTargets);
            size_t wStride = accStride(model, outAcc, sizeof(float));
            for (uint32_t k = 0; k < numKeys; ++k) {
                memcpy(&mc.times[k], tsData + k * tsStride, sizeof(float));
                for (uint32_t t2 = 0; t2 < numTargets; ++t2) {
                    size_t idx = (size_t)k * numTargets + t2;
                    memcpy(&mc.weights[idx], outData + idx * wStride, sizeof(float));
                }
            }
        }
    }

    AnimHeader hdr;
    hdr.duration = duration;
    hdr.numChannels = (uint32_t)channels.size();
    hdr.numMorphChannels = (uint32_t)morphCh.size();

    std::vector<char> payload;
    auto appendRaw = [&](const void* ptr, size_t bytes) {
        payload.insert(payload.end(), (const char*)ptr, (const char*)ptr + bytes);
        };

    for (auto& [name, c] : channels) {
        writeStr(payload, name);
        appendRaw(&c.numPositions, 4);
        if (c.numPositions) {
            appendRaw(c.posTimeStamps.get(), c.numPositions * sizeof(float));
            appendRaw(c.positions.get(), c.numPositions * sizeof(Vector3));
        }
        appendRaw(&c.numRotations, 4);
        if (c.numRotations) {
            appendRaw(c.rotTimeStamps.get(), c.numRotations * sizeof(float));
            appendRaw(c.rotations.get(), c.numRotations * sizeof(Quaternion));
        }
    }
    for (auto& [name, mc] : morphCh) {
        writeStr(payload, name);
        appendRaw(&mc.numKeyframes, 4);
        appendRaw(&mc.numTargets, 4);
        if (mc.numKeyframes) {
            appendRaw(mc.times.get(), mc.numKeyframes * sizeof(float));
            appendRaw(mc.weights.get(), (size_t)mc.numKeyframes * mc.numTargets * sizeof(float));
        }
    }
    return ImporterUtils::SaveBuffer(outputFile, hdr, payload);
}

bool AnimImporter::Load(const std::string& file, ResourceAnimation& out) {
    AnimHeader hdr;
    std::vector<char> raw;
    if (!ImporterUtils::LoadBuffer(file, hdr, raw)) return false;
    if (hdr.magic != 0x4E494D41) {
        LOG("AnimImporter: Bad magic in %s", file.c_str()); return false;
    }
    out.duration = hdr.duration;
    out.channels.clear();
    out.morphChannels.clear();

    const char* cur = raw.data() + sizeof(AnimHeader);

    for (uint32_t i = 0; i < hdr.numChannels; ++i) {
        std::string name = readStr(cur);
        AnimChannel c;
        memcpy(&c.numPositions, cur, 4); cur += 4;
        if (c.numPositions) {
            c.posTimeStamps = std::make_unique<float[]>(c.numPositions);
            c.positions = std::make_unique<Vector3[]>(c.numPositions);
            memcpy(c.posTimeStamps.get(), cur, c.numPositions * sizeof(float));
            cur += c.numPositions * sizeof(float);
            memcpy(c.positions.get(), cur, c.numPositions * sizeof(Vector3));
            cur += c.numPositions * sizeof(Vector3);
        }
        memcpy(&c.numRotations, cur, 4); cur += 4;
        if (c.numRotations) {
            c.rotTimeStamps = std::make_unique<float[]>(c.numRotations);
            c.rotations = std::make_unique<Quaternion[]>(c.numRotations);
            memcpy(c.rotTimeStamps.get(), cur, c.numRotations * sizeof(float));
            cur += c.numRotations * sizeof(float);
            memcpy(c.rotations.get(), cur, c.numRotations * sizeof(Quaternion));
            cur += c.numRotations * sizeof(Quaternion);
        }
        out.channels[name] = std::move(c);
    }
    for (uint32_t i = 0; i < hdr.numMorphChannels; ++i) {
        std::string name = readStr(cur);
        MorphChannel mc;
        memcpy(&mc.numKeyframes, cur, 4); cur += 4;
        memcpy(&mc.numTargets, cur, 4); cur += 4;
        if (mc.numKeyframes) {
            mc.times = std::make_unique<float[]>(mc.numKeyframes);
            mc.weights = std::make_unique<float[]>((size_t)mc.numKeyframes * mc.numTargets);
            memcpy(mc.times.get(), cur, mc.numKeyframes * sizeof(float));
            cur += mc.numKeyframes * sizeof(float);
            size_t wb = (size_t)mc.numKeyframes * mc.numTargets * sizeof(float);
            memcpy(mc.weights.get(), cur, wb); cur += wb;
        }
        out.morphChannels[name] = std::move(mc);
    }
    return true;
}
