#include "Globals.h"
#include "MaterialImporter.h"
#include "TextureImporter.h"
#include "Application.h"
#include "ModuleFileSystem.h"

bool MaterialImporter::Import(
    const tinygltf::Material& gltfMaterial,
    const tinygltf::Model& model,
    const char* basePath,
    const char* libraryPath,
    UID& outMaterialUID)
{
    MaterialBinary mat{};
    mat.materialUID = GenerateUID();

    const auto& pbr = gltfMaterial.pbrMetallicRoughness;

    mat.baseColor[0] = (float)pbr.baseColorFactor[0];
    mat.baseColor[1] = (float)pbr.baseColorFactor[1];
    mat.baseColor[2] = (float)pbr.baseColorFactor[2];
    mat.baseColor[3] = (float)pbr.baseColorFactor[3];

    mat.hasTexture = 0;
    mat.textureUID = 0;

    if (pbr.baseColorTexture.index >= 0)
    {
        const auto& tex = model.textures[pbr.baseColorTexture.index];
        const auto& img = model.images[tex.source];

        std::string fullPath =
            std::string(basePath) + img.uri;

        mat.textureUID =
            TextureImporter::Import(
                fullPath.c_str(),
                "Library/Textures/"
            );

        mat.hasTexture = mat.textureUID != 0;
    }

    std::string outPath =
        std::string(libraryPath) +
        std::to_string(mat.materialUID) + ".mat";

    outMaterialUID = mat.materialUID;

    return app->getFileSystem()->Save(
        outPath.c_str(),
        &mat,
        sizeof(mat)
    );
}
