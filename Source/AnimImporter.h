#pragma once
#include <string>
#include <memory>
class ResourceAnimation;
namespace tinygltf { class Model; }

class AnimImporter {
public:
    static bool Import(const tinygltf::Model& model, int animIndex, const std::string& outputFile);

    static bool Load(const std::string& file, ResourceAnimation& outAnim);

    struct AnimHeader {
        uint32_t magic = 0x4E494D41;
        uint32_t version = 1;
        float    duration = 0.f;
        uint32_t numChannels = 0;
        uint32_t numMorphChannels = 0;
    };
};
