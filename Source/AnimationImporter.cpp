#include "Globals.h"
#include "AnimationImporter.h"
#include "ImporterUtils.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "gltf_utils.h"
#include <unordered_map>
#include <algorithm>
#include <cstring>

// Binary format written by this file and read by ResourceAnimation::LoadInMemory.
//
// [AnimFileHeader]   (version 2)
// char[animNameLen]
// For each transform channel (channelCount):
//   uint32 nodeNameLen | uint32 posCount | uint32 rotCount
//   char[nodeNameLen]
//   float[posCount]       posTimeStamps
//   Vector3[posCount]     positions
//   float[rotCount]       rotTimeStamps
//   Quaternion[rotCount]  rotations
// uint32 morphChannelCount
// For each morph channel (morphChannelCount):
//   uint32 nodeNameLen | uint32 numTime | uint32 numTargets
//   char[nodeNameLen]
//   float[numTime]              weightsTimes
//   float[numTime * numTargets] weights

namespace {

struct AnimFileHeader {
    uint32_t magic        = 0x414E494D; // 'ANIM'
    uint32_t version      = 2;          // v2 adds morph-channel section after transform channels
    uint32_t animNameLen  = 0;
    uint32_t channelCount = 0;
    float    duration     = 0.f;
};

struct NodeAnim {
    std::string               name;
    std::unique_ptr<float[]>      posTimes;
    std::unique_ptr<Vector3[]>    positions;
    UINT                          posCount = 0;
    std::unique_ptr<float[]>      rotTimes;
    std::unique_ptr<Quaternion[]> rotations;
    UINT                          rotCount = 0;
};

struct NodeMorph {
    std::string              name;
    std::unique_ptr<float[]> times;
    std::unique_ptr<float[]> weights;   // flat: [t0_w0, t0_w1, ..., t1_w0, ...]
    uint32_t                 numTime    = 0;
    uint32_t                 numTargets = 0;
};

static bool importOne(const tinygltf::Model& gltfModel, int animIdx,
                      const std::string& sceneName, const std::string& outPath) {
    const auto& anim = gltfModel.animations[animIdx];

    auto getNodeName = [&](int idx) -> std::string {
        if (idx < 0 || idx >= (int)gltfModel.nodes.size()) return "Node_" + std::to_string(idx);
        const auto& n = gltfModel.nodes[idx];
        return n.name.empty() ? ("Node_" + std::to_string(idx)) : n.name;
    };

    // One NodeAnim per node, merging translation and rotation channels.
    std::unordered_map<int, NodeAnim> nodeMap;
    // One NodeMorph per node for weight channels (glTF path == "weights").
    std::unordered_map<int, NodeMorph> morphMap;
    float duration = 0.f;

    for (const auto& chan : anim.channels) {
        if (chan.target_node < 0 ||
            chan.sampler < 0 || chan.sampler >= (int)anim.samplers.size()) continue;

        const auto& sampler = anim.samplers[chan.sampler];
        if (sampler.input < 0 || sampler.output < 0) continue;

        if (chan.target_path == "translation" || chan.target_path == "rotation") {
            NodeAnim& na = nodeMap[chan.target_node];
            if (na.name.empty()) na.name = getNodeName(chan.target_node);

            if (chan.target_path == "translation") {
                UINT timeCnt = 0, valCnt = 0;
                std::unique_ptr<float[]>   times;
                std::unique_ptr<Vector3[]> values;

                if (!loadAccessorTyped(times,  timeCnt, gltfModel, sampler.input))  continue;
                if (!loadAccessorTyped(values, valCnt,  gltfModel, sampler.output)) continue;

                for (UINT i = 0; i < timeCnt; ++i) duration = std::max(duration, times[i]);
                na.posTimes  = std::move(times);
                na.positions = std::move(values);
                na.posCount  = timeCnt;

            } else { // rotation
                UINT timeCnt = 0, valCnt = 0;
                std::unique_ptr<float[]>      times;
                std::unique_ptr<Quaternion[]> values;

                if (!loadAccessorTyped(times,  timeCnt, gltfModel, sampler.input))  continue;
                if (!loadAccessorTyped(values, valCnt,  gltfModel, sampler.output)) continue;

                for (UINT i = 0; i < timeCnt; ++i) duration = std::max(duration, times[i]);
                na.rotTimes   = std::move(times);
                na.rotations  = std::move(values);
                na.rotCount   = timeCnt;
            }

        } else if (chan.target_path == "weights") {
            UINT timeCnt = 0, valCnt = 0;
            std::unique_ptr<float[]> times, values;

            if (!loadAccessorTyped(times,  timeCnt, gltfModel, sampler.input))  continue;
            if (!loadAccessorTyped(values, valCnt,  gltfModel, sampler.output)) continue;
            if (timeCnt == 0 || valCnt == 0) continue;

            const uint32_t numTargets = valCnt / timeCnt;
            if (numTargets == 0) continue;

            // Warn if the target count doesn't match the mesh's morph target count.
            int nodeIdx = chan.target_node;
            if (nodeIdx >= 0 && nodeIdx < (int)gltfModel.nodes.size()) {
                int meshIdx = gltfModel.nodes[nodeIdx].mesh;
                if (meshIdx >= 0 && meshIdx < (int)gltfModel.meshes.size()) {
                    const auto& gltfMesh = gltfModel.meshes[meshIdx];
                    if (!gltfMesh.primitives.empty()) {
                        size_t meshTargets = gltfMesh.primitives[0].targets.size();
                        if (meshTargets > 0 && numTargets != (uint32_t)meshTargets) {
                            LOG("AnimationImporter: weights channel for node %d has %u targets but mesh has %zu",
                                nodeIdx, numTargets, meshTargets);
                        }
                    }
                }
            }

            for (UINT i = 0; i < timeCnt; ++i) duration = std::max(duration, times[i]);

            NodeMorph& nm = morphMap[chan.target_node];
            if (nm.name.empty()) nm.name = getNodeName(chan.target_node);
            nm.times      = std::move(times);
            nm.weights    = std::move(values);
            nm.numTime    = timeCnt;
            nm.numTargets = numTargets;
        }
    }

    // Count channels that carry at least one type of keyframe.
    uint32_t validCount = 0;
    for (const auto& [idx, na] : nodeMap)
        if (na.posCount > 0 || na.rotCount > 0) ++validCount;

    uint32_t validMorphCount = 0;
    for (const auto& [idx, nm] : morphMap)
        if (nm.numTime > 0) ++validMorphCount;

    if (validCount == 0 && validMorphCount == 0) return false;

    std::string animName = anim.name.empty() ? ("Anim_" + std::to_string(animIdx)) : anim.name;

    AnimFileHeader header;
    header.animNameLen  = (uint32_t)animName.size();
    header.channelCount = validCount;
    header.duration     = duration;

    std::vector<char> payload;
    auto append = [&](const void* d, size_t n) {
        const char* p = static_cast<const char*>(d);
        payload.insert(payload.end(), p, p + n);
    };

    append(animName.data(), animName.size());

    for (const auto& [idx, na] : nodeMap) {
        if (na.posCount == 0 && na.rotCount == 0) continue;

        uint32_t nameLen  = (uint32_t)na.name.size();
        uint32_t posCount = na.posCount;
        uint32_t rotCount = na.rotCount;

        append(&nameLen,  sizeof(uint32_t));
        append(&posCount, sizeof(uint32_t));
        append(&rotCount, sizeof(uint32_t));
        append(na.name.data(), nameLen);

        if (posCount > 0) {
            append(na.posTimes.get(),  posCount * sizeof(float));
            append(na.positions.get(), posCount * sizeof(Vector3));
        }
        if (rotCount > 0) {
            append(na.rotTimes.get(),   rotCount * sizeof(float));
            append(na.rotations.get(),  rotCount * sizeof(Quaternion));
        }
    }

    // Serialize morph channels (version 2 extension)
    uint32_t morphChannelCount = validMorphCount;
    append(&morphChannelCount, sizeof(uint32_t));
    for (const auto& [idx, nm] : morphMap) {
        if (nm.numTime == 0) continue;
        uint32_t nameLen    = (uint32_t)nm.name.size();
        uint32_t numTime    = nm.numTime;
        uint32_t numTargets = nm.numTargets;
        append(&nameLen,    sizeof(uint32_t));
        append(&numTime,    sizeof(uint32_t));
        append(&numTargets, sizeof(uint32_t));
        append(nm.name.data(), nameLen);
        append(nm.times.get(),   numTime * sizeof(float));
        append(nm.weights.get(), numTime * numTargets * sizeof(float));
    }

    return ImporterUtils::SaveBuffer(outPath, header, payload);
}

} // namespace

int AnimationImporter::ImportAll(const tinygltf::Model& gltfModel, const std::string& sceneName) {
    ModuleFileSystem* fs = app->getFileSystem();
    std::string animsDir = fs->GetLibraryPath() + "Animations/";
    std::string sceneDir = animsDir + sceneName;
    fs->CreateDir(animsDir.c_str());
    fs->CreateDir(sceneDir.c_str());

    int count = 0;
    for (int i = 0; i < (int)gltfModel.animations.size(); ++i) {
        std::string outPath = ImporterUtils::IndexedPath(sceneDir, i, ".anim");
        if (importOne(gltfModel, i, sceneName, outPath))
            ++count;
        else
            LOG("AnimationImporter: Skipped animation %d for '%s' (no keyframes)", i, sceneName.c_str());
    }
    return count;
}
