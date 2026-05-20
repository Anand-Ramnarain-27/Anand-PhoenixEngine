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
// [AnimFileHeader]
// char[animNameLen]
// For each channel:
//   uint32 nodeNameLen | uint32 posCount | uint32 rotCount
//   char[nodeNameLen]
//   float[posCount]       posTimeStamps
//   Vector3[posCount]     positions
//   float[rotCount]       rotTimeStamps
//   Quaternion[rotCount]  rotations

namespace {

struct AnimFileHeader {
    uint32_t magic       = 0x414E494D; // 'ANIM'
    uint32_t version     = 1;
    uint32_t animNameLen = 0;
    uint32_t channelCount = 0;
    float    duration    = 0.f;
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
    float duration = 0.f;

    for (const auto& chan : anim.channels) {
        if (chan.target_node < 0 ||
            chan.sampler < 0 || chan.sampler >= (int)anim.samplers.size()) continue;

        const auto& sampler = anim.samplers[chan.sampler];
        if (sampler.input < 0 || sampler.output < 0) continue;

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

        } else if (chan.target_path == "rotation") {
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
    }

    // Count channels that carry at least one type of keyframe.
    uint32_t validCount = 0;
    for (const auto& [idx, na] : nodeMap)
        if (na.posCount > 0 || na.rotCount > 0) ++validCount;

    if (validCount == 0) return false;

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
