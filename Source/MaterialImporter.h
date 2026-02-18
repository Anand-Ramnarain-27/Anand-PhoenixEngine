#pragma once

#include <string>
#include <memory>

class Material;
namespace tinygltf { class Model; struct Material; }

class MaterialImporter
{
public:
    struct MaterialHeader
    {
        uint32_t magic = 0x4D415452;
        uint32_t version = 1;
        uint32_t hasTexture = 0;
        uint32_t texturePathLength = 0;
    };

    static bool Import(const tinygltf::Material& gltfMaterial, const tinygltf::Model& model,
        const std::string& sceneName, const std::string& outputFile,
        int materialIndex, const std::string& basePath);

    static bool Load(const std::string& file, std::unique_ptr<Material>& outMaterial);

private:
    static bool Save(const MaterialHeader& header, const Material* material,
        const std::string& texturePath, const std::string& file);
};