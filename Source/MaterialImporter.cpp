#include "Globals.h"
#include "MaterialImporter.h"
#include "TextureImporter.h"
#include "Material.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "tiny_gltf.h"
#include <filesystem>
#include <cstring>

bool MaterialImporter::Import(const tinygltf::Material& gltfMat, const tinygltf::Model& model,
    const std::string& sceneName, const std::string& outputFile,
    int materialIndex, const std::string& basePath)
{
    ModuleFileSystem* fs = app->getFileSystem();

    auto mat = std::make_unique<Material>();
    const auto& pbr = gltfMat.pbrMetallicRoughness;

    mat->getData().baseColor = Vector4(float(pbr.baseColorFactor[0]), float(pbr.baseColorFactor[1]),
        float(pbr.baseColorFactor[2]), float(pbr.baseColorFactor[3]));
    mat->getData().metallic = float(pbr.metallicFactor);
    mat->getData().roughness = float(pbr.roughnessFactor);

    MaterialHeader header;
    std::string    texturePath;

    if (pbr.baseColorTexture.index >= 0 && pbr.baseColorTexture.index < (int)model.textures.size())
    {
        const auto& tex = model.textures[pbr.baseColorTexture.index];
        if (tex.source >= 0 && tex.source < (int)model.images.size())
        {
            const std::string& uri = model.images[tex.source].uri;
            if (!uri.empty())
            {
                std::string srcPath = basePath + uri;
                std::string texName = TextureImporter::GetTextureName(uri.c_str());
                std::string matFolder = fs->GetLibraryPath() + "Materials/" + sceneName;
                std::string ddsPath = matFolder + "/" + texName + ".dds";

                bool imported = fs->Exists(ddsPath.c_str()) || TextureImporter::Import(srcPath.c_str(), ddsPath);
                if (imported)
                {
                    header.hasTexture = 1;
                    texturePath = ddsPath;
                    mat->getData().hasBaseColorTexture = 1;
                }
                else
                {
                    LOG("MaterialImporter: Failed to import texture %s", srcPath.c_str());
                }
            }
        }
    }

    header.texturePathLength = (uint32_t)texturePath.size();
    return Save(header, mat.get(), texturePath, outputFile);
}

bool MaterialImporter::Load(const std::string& file, std::unique_ptr<Material>& outMaterial)
{
    char* buffer = nullptr;
    uint32_t fileSize = app->getFileSystem()->Load(file.c_str(), &buffer);

    if (!buffer || fileSize < sizeof(MaterialHeader))
    {
        delete[] buffer;
        return false;
    }

    MaterialHeader header;
    const char* cursor = buffer;
    memcpy(&header, cursor, sizeof(header));
    cursor += sizeof(header);

    if (header.magic != 0x4D415452 || header.version != 1)
    {
        delete[] buffer;
        LOG("MaterialImporter: Invalid file format");
        return false;
    }

    std::string texturePath;
    if (header.hasTexture && header.texturePathLength > 0)
    {
        texturePath.assign(cursor, header.texturePathLength);
    }
    delete[] buffer;

    outMaterial = std::make_unique<Material>();

    if (!texturePath.empty())
    {
        ComPtr<ID3D12Resource>      texture;
        D3D12_GPU_DESCRIPTOR_HANDLE srv;

        if (TextureImporter::Load(texturePath, texture, srv))
            outMaterial->setBaseColorTexture(texture, srv);
        else
            LOG("MaterialImporter: Failed to load texture %s", texturePath.c_str());
    }

    return true;
}

bool MaterialImporter::Save(const MaterialHeader& header, const Material* /*material*/,
    const std::string& texturePath, const std::string& file)
{
    uint32_t totalSize = sizeof(MaterialHeader) + header.texturePathLength;

    std::vector<char> buffer(totalSize);
    char* cursor = buffer.data();

    memcpy(cursor, &header, sizeof(header));
    cursor += sizeof(header);

    if (header.hasTexture && header.texturePathLength > 0)
        memcpy(cursor, texturePath.c_str(), header.texturePathLength);

    if (!app->getFileSystem()->Save(file.c_str(), buffer.data(), totalSize))
    {
        LOG("MaterialImporter: Failed to save %s", file.c_str());
        return false;
    }

    return true;
}