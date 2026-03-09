#pragma once

#include <string>
#include <memory>

class Material;
namespace tinygltf { class Model; struct Material; }

class MaterialImporter {
public:
    struct MaterialHeader {
        uint32_t magic = 0x4D415452;
        uint32_t version = 2;           

        uint32_t hasTexture = 0;
        uint32_t texturePathLength = 0;

        uint32_t hasNormalMap = 0;
        uint32_t normalPathLength = 0;

        uint32_t hasAOMap = 0;
        uint32_t aoPathLength = 0;

        uint32_t hasEmissiveMap = 0;
        uint32_t emissivePathLength = 0;

        float    normalStrength = 1.f;
        float    aoStrength = 1.f;
        float    emissiveR = 1.f;
        float    emissiveG = 1.f;
        float    emissiveB = 1.f;
        float    metallic = 0.f;
        float    roughness = 0.5f;
        float    _pad = 0.f;
    };

    static bool Import(const tinygltf::Material& gltfMaterial,
        const tinygltf::Model& model,
        const std::string& sceneName,
        const std::string& outputFile,
        int                       materialIndex,
        const std::string& basePath);

    static bool Load(const std::string& file, std::unique_ptr<Material>& outMaterial);

private:
    static bool Save(const MaterialHeader& header,
        const std::string& baseColorPath,
        const std::string& normalPath,
        const std::string& aoPath,
        const std::string& emissivePath,
        const std::string& file);

    static std::string importTexture(int texIndex,
        const tinygltf::Model& model,
        const std::string& sceneName,
        const std::string& basePath);
};