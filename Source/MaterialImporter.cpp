#include "Globals.h"
#include "MaterialImporter.h"
#include "TextureImporter.h"
#include "Material.h"
#include "Application.h"
#include "ModuleFileSystem.h"

//#define TINYGLTF_NO_STB_IMAGE_WRITE
//#define TINYGLTF_NO_STB_IMAGE
//#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "tiny_gltf.h"

#include <cstring>
#include <filesystem>

bool MaterialImporter::Import(const tinygltf::Material& gltfMaterial, const tinygltf::Model& model, const std::string& sceneName, const std::string& outputFile, int materialIndex)
{
    LOG("MaterialImporter: Importing material %d", materialIndex);

    ModuleFileSystem* fs = app->getFileSystem();

    auto tempMaterial = std::make_unique<Material>();

    std::string basePath = fs->GetAssetsPath();

    // Load material data using existing Material::load
    // Note: This won't create GPU resources, just extract the data
    // tempMaterial->load(gltfMaterial, model, basePath.c_str());

    MaterialHeader header;
    std::string texturePath;

    const auto& pbr = gltfMaterial.pbrMetallicRoughness;
    if (pbr.baseColorTexture.index >= 0 && pbr.baseColorTexture.index < (int)model.textures.size())
    {
        const auto& texture = model.textures[pbr.baseColorTexture.index];
        if (texture.source >= 0 && texture.source < (int)model.images.size())
        {
            const auto& image = model.images[texture.source];

            if (!image.uri.empty())
            {
                std::string sourceTexPath = basePath + image.uri;
                std::string textureName = TextureImporter::GetTextureName(image.uri.c_str());

                std::string materialFolder = fs->GetLibraryPath() + "Materials/" + sceneName;
                std::string ddsPath = materialFolder + "/" + textureName + ".dds";

                if (!fs->Exists(ddsPath.c_str()))
                {
                    if (TextureImporter::Import(sourceTexPath.c_str(), ddsPath))
                    {
                        LOG("MaterialImporter: Imported texture %s", textureName.c_str());
                        header.hasTexture = 1;
                        texturePath = ddsPath;
                    }
                }
                else
                {
                    LOG("MaterialImporter: Texture already exists: %s", ddsPath.c_str());
                    header.hasTexture = 1;
                    texturePath = ddsPath;
                }
            }
        }
    }

    header.texturePathLength = (uint32_t)texturePath.length();

    return Save(header, tempMaterial.get(), texturePath, outputFile);
}

bool MaterialImporter::Load(const std::string& file, std::unique_ptr<Material>& outMaterial)
{
    ModuleFileSystem* fs = app->getFileSystem();

    char* buffer = nullptr;
    uint32_t fileSize = fs->Load(file.c_str(), &buffer);

    if (!buffer || fileSize < sizeof(MaterialHeader))
    {
        if (buffer) delete[] buffer;
        return false;
    }

    char* cursor = buffer;

    MaterialHeader header;
    memcpy(&header, cursor, sizeof(MaterialHeader));
    cursor += sizeof(MaterialHeader);

    if (header.magic != 0x4D415452 || header.version != 1)
    {
        delete[] buffer;
        LOG("MaterialImporter: Invalid material file format");
        return false;
    }

    std::string texturePath;
    if (header.hasTexture && header.texturePathLength > 0)
    {
        texturePath.resize(header.texturePathLength);
        memcpy(&texturePath[0], cursor, header.texturePathLength);
        cursor += header.texturePathLength;

        LOG("MaterialImporter: Material references texture: %s", texturePath.c_str());
    }

    delete[] buffer;

    outMaterial = std::make_unique<Material>();

    // load the texture and set it to the material
    // This would integrate with your ModuleResources
    // ComPtr<ID3D12Resource> texture;
    // if (!texturePath.empty())
    // {
    //     texture = app->getResources()->createTextureFromFile(texturePath, true);
    // }

    LOG("MaterialImporter: Successfully loaded material from %s", file.c_str());
    return true;
}

bool MaterialImporter::Save(const MaterialHeader& header, const Material* material, const std::string& texturePath, const std::string& file)
{
    ModuleFileSystem* fs = app->getFileSystem();

    uint32_t totalSize = sizeof(MaterialHeader) + header.texturePathLength;

    std::vector<char> buffer(totalSize);
    char* cursor = buffer.data();

    memcpy(cursor, &header, sizeof(MaterialHeader));
    cursor += sizeof(MaterialHeader);

    if (header.hasTexture && header.texturePathLength > 0)
    {
        memcpy(cursor, texturePath.c_str(), header.texturePathLength);
        cursor += header.texturePathLength;
    }

    if (!fs->Save(file.c_str(), buffer.data(), totalSize))
    {
        LOG("MaterialImporter: Failed to save material to %s", file.c_str());
        return false;
    }

    LOG("MaterialImporter: Saved material to %s", file.c_str());
    return true;
}