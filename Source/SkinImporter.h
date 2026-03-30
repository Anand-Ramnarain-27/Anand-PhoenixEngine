#pragma once
#include <string>
class ResourceSkin;
namespace tinygltf { class Model; }

class SkinImporter {
public:
    struct SkinHeader {
        uint32_t magic = 0x4E494B53; 
        uint32_t version = 1;
        uint32_t jointCount = 0;
    };
    static bool Import(const tinygltf::Model& model, int skinIndex,
        const std::string& outputFile);
    static bool Load(const std::string& file, ResourceSkin& out);
};
