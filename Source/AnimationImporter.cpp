#include "Globals.h"
#include "AnimationImporter.h"
#include "ImporterUtils.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "gltf_utils.h"
#include <unordered_map>
#include <algorithm>
#include <cstring>


namespace {

struct AnimFileHeader {
    uint32_t magic = 0x414E494D;
    uint32_t version = 2;
    uint32_t animNameLen = 0;
    uint32_t channelCount = 0;
    float duration = 0.f;
};

struct NodeAnim {
    std::string name;
    std::unique_ptr<float[]> posTimes;
    std::unique_ptr<Vector3[]> positions;
    UINT posCount = 0;
    std::unique_ptr<float[]> rotTimes;
    std::unique_ptr<Quaternion[]> rotations;
    UINT rotCount = 0;
};

struct NodeMorph {
    std::string name;
    std::unique_ptr<float[]> times;
    std::unique_ptr<float[]> weights;
    uint32_t numTime = 0;
    uint32_t numTargets = 0;
};

static bool importOne(const tinygltf::Model& gltfModel, int animIdx,
                      const std::string& sceneName, const std::string& outPath){
    const auto& anim = gltfModel.animations[animIdx];

    auto getNodeName = [&](int idx) -> std::string {
        if (idx < 0 || idx >= (int)gltfModel.nodes.size()) return "Node_" + std::to_string(idx);
        const auto& n = gltfModel.nodes[idx];
        return n.name.empty() ? ("Node_" + std::to_string(idx)) : n.name;
    };

    std::unordered_map<int, NodeAnim> nodeMap;
    std::unordered_map<int, NodeMorph> morphMap;
    float duration = 0.f;

    for (const auto& chan : anim.channels){
        if (chan.target_node < 0 ||
            chan.sampler < 0 || chan.sampler >= (int)anim.samplers.size()) continue;

        const auto& sampler = anim.samplers[chan.sampler];
        if (sampler.input < 0 || sampler.output < 0) continue;

        if (chan.target_path == "translation" || chan.target_path == "rotation"){
            NodeAnim& na = nodeMap[chan.target_node];
            if (na.name.empty()) na.name = getNodeName(chan.target_node);

            if (chan.target_path == "translation"){
                UINT timeCnt = 0, valCnt = 0;
                std::unique_ptr<float[]> times;
                std::unique_ptr<Vector3[]> values;

                if (!loadAccessorTyped(times, timeCnt, gltfModel, sampler.input)) continue;
                if (!loadAccessorTyped(values, valCnt, gltfModel, sampler.output)) continue;

                for (UINT i = 0; i < timeCnt; ++i) duration = std::max(duration, times[i]);
                na.posTimes = std::move(times);
                na.positions = std::move(values);
                na.posCount = timeCnt;

            } else {
                UINT timeCnt = 0, valCnt = 0;
                std::unique_ptr<float[]> times;
                std::unique_ptr<Quaternion[]> values;

                if (!loadAccessorTyped(times, timeCnt, gltfModel, sampler.input)) continue;
                if (!loadAccessorTyped(values, valCnt, gltfModel, sampler.output)) continue;

                for (UINT i = 0; i < timeCnt; ++i) duration = std::max(duration, times[i]);
                na.rotTimes = std::move(times);
                na.rotations = std::move(values);
                na.rotCount = timeCnt;
            }

        } else if (chan.target_path == "weights"){
            UINT timeCnt = 0, valCnt = 0;
            std::unique_ptr<float[]> times, values;

            if (!loadAccessorTyped(times, timeCnt, gltfModel, sampler.input)) continue;

            {
                const tinygltf::Accessor& outAcc = gltfModel.accessors[sampler.output];
                valCnt = (UINT)outAcc.count;
                const int compType = outAcc.componentType;
                const bool normalized = outAcc.normalized;
                const bool isFloat = (compType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                if (isFloat){
                    values = std::make_unique<float[]>(valCnt);
                    if (!loadAccessorData(reinterpret_cast<uint8_t*>(values.get()),
                                          sizeof(float), sizeof(float), valCnt, gltfModel, sampler.output))
                        continue;
                } else {
                    size_t compSize = tinygltf::GetComponentSizeInBytes(compType);
                    std::vector<uint8_t> raw(valCnt * compSize);
                    if (!loadAccessorData(raw.data(), compSize, compSize, valCnt, gltfModel, sampler.output))
                        continue;
                    values = std::make_unique<float[]>(valCnt);
                    for (UINT vi = 0; vi < valCnt; ++vi){
                        const uint8_t* p = raw.data() + vi * compSize;
                        float f = 0.f;
                        if (compType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE){
                            f = normalized ? (*p / 255.0f) : (float)*p;
                        } else if (compType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT){
                            uint16_t v16; memcpy(&v16, p, 2);
                            f = normalized ? (v16 / 65535.0f) : (float)v16;
                        } else if (compType == TINYGLTF_COMPONENT_TYPE_BYTE){
                            int8_t s8; memcpy(&s8, p, 1);
                            f = normalized ? std::max(s8 / 127.0f, -1.0f) : (float)s8;
                        } else if (compType == TINYGLTF_COMPONENT_TYPE_SHORT){
                            int16_t s16; memcpy(&s16, p, 2);
                            f = normalized ? std::max(s16 / 32767.0f, -1.0f) : (float)s16;
                        } else {
                            f = (float)*p;
                        }
                        values[vi] = f;
                    }
                    LOG("AnimationImporter: quantized weights accessor (compType=%d normalized=%d) — converted %u values to float",
                        compType, (int)normalized, valCnt);
                }
            }
            if (timeCnt == 0 || valCnt == 0) continue;

            const uint32_t numTargets = valCnt / timeCnt;
            if (numTargets == 0) continue;

            int nodeIdx = chan.target_node;
            if (nodeIdx >= 0 && nodeIdx < (int)gltfModel.nodes.size()){
                int meshIdx = gltfModel.nodes[nodeIdx].mesh;
                if (meshIdx >= 0 && meshIdx < (int)gltfModel.meshes.size()){
                    const auto& gltfMesh = gltfModel.meshes[meshIdx];
                    if (!gltfMesh.primitives.empty()){
                        size_t meshTargets = gltfMesh.primitives[0].targets.size();
                        if (meshTargets > 0 && numTargets != (uint32_t)meshTargets){
                            LOG("AnimationImporter: weights channel for node %d has %u targets but mesh has %zu",
                                nodeIdx, numTargets, meshTargets);
                        }
                    }
                }
            }

            for (UINT i = 0; i < timeCnt; ++i) duration = std::max(duration, times[i]);

            NodeMorph& nm = morphMap[chan.target_node];
            if (nm.name.empty()) nm.name = getNodeName(chan.target_node);
            nm.times = std::move(times);
            nm.weights = std::move(values);
            nm.numTime = timeCnt;
            nm.numTargets = numTargets;
        }
    }

    uint32_t validCount = 0;
    for (const auto& [idx, na] : nodeMap)
        if (na.posCount > 0 || na.rotCount > 0) ++validCount;

    uint32_t validMorphCount = 0;
    for (const auto& [idx, nm] : morphMap)
        if (nm.numTime > 0) ++validMorphCount;

    if (validCount == 0 && validMorphCount == 0) return false;

    std::string animName = anim.name.empty() ? ("Anim_" + std::to_string(animIdx)) : anim.name;

    AnimFileHeader header;
    header.animNameLen = (uint32_t)animName.size();
    header.channelCount = validCount;
    header.duration = duration;

    std::vector<char> payload;
    auto append = [&](const void* d, size_t n){
        const char* p = static_cast<const char*>(d);
        payload.insert(payload.end(), p, p + n);
    };

    append(animName.data(), animName.size());

    for (const auto& [idx, na] : nodeMap){
        if (na.posCount == 0 && na.rotCount == 0) continue;

        uint32_t nameLen = (uint32_t)na.name.size();
        uint32_t posCount = na.posCount;
        uint32_t rotCount = na.rotCount;

        append(&nameLen, sizeof(uint32_t));
        append(&posCount, sizeof(uint32_t));
        append(&rotCount, sizeof(uint32_t));
        append(na.name.data(), nameLen);

        if (posCount > 0){
            append(na.posTimes.get(), posCount * sizeof(float));
            append(na.positions.get(), posCount * sizeof(Vector3));
        }
        if (rotCount > 0){
            append(na.rotTimes.get(), rotCount * sizeof(float));
            append(na.rotations.get(), rotCount * sizeof(Quaternion));
        }
    }

    uint32_t morphChannelCount = validMorphCount;
    append(&morphChannelCount, sizeof(uint32_t));
    for (const auto& [idx, nm] : morphMap){
        if (nm.numTime == 0) continue;
        uint32_t nameLen = (uint32_t)nm.name.size();
        uint32_t numTime = nm.numTime;
        uint32_t numTargets = nm.numTargets;
        append(&nameLen, sizeof(uint32_t));
        append(&numTime, sizeof(uint32_t));
        append(&numTargets, sizeof(uint32_t));
        append(nm.name.data(), nameLen);
        append(nm.times.get(), numTime * sizeof(float));
        append(nm.weights.get(), numTime * numTargets * sizeof(float));
    }

    return ImporterUtils::SaveBuffer(outPath, header, payload);
}

}

int AnimationImporter::ImportAll(const tinygltf::Model& gltfModel, const std::string& sceneName){
    ModuleFileSystem* fs = app->getFileSystem();
    std::string animsDir = fs->GetLibraryPath() + "Animations/";
    std::string sceneDir = animsDir + sceneName;
    fs->CreateDir(animsDir.c_str());
    fs->CreateDir(sceneDir.c_str());

    LOG("AnimationImporter: '%s' — %d animation(s) in glTF", sceneName.c_str(), (int)gltfModel.animations.size());

    int count = 0;
    for (int i = 0; i < (int)gltfModel.animations.size(); ++i){
        const auto& anim = gltfModel.animations[i];

        std::string channelSummary;
        for (const auto& ch : anim.channels){
            const std::string& nodeName = (ch.target_node >= 0 && ch.target_node < (int)gltfModel.nodes.size())
                ? (gltfModel.nodes[ch.target_node].name.empty()
                    ? ("Node_" + std::to_string(ch.target_node))
                    : gltfModel.nodes[ch.target_node].name)
                : "?";
            channelSummary += nodeName + ":" + ch.target_path + " ";
        }
        LOG("AnimationImporter: anim[%d] '%s' — %d channel(s): %s",
            i, anim.name.c_str(), (int)anim.channels.size(), channelSummary.c_str());

        std::string outPath = ImporterUtils::IndexedPath(sceneDir, i, ".anim");
        if (importOne(gltfModel, i, sceneName, outPath))
            ++count;
        else
            LOG("AnimationImporter: Skipped animation %d '%s' for '%s' (no valid keyframes)",
                i, anim.name.c_str(), sceneName.c_str());
    }
    LOG("AnimationImporter: '%s' — wrote %d .anim file(s)", sceneName.c_str(), count);
    return count;
}
