#include "Globals.h"
#include "MaterialImporter.h"
#include "TextureImporter.h"
#include "Material.h"
#include "ImporterUtils.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "tiny_gltf.h"
#include <filesystem>
#include <cstring>

bool MaterialImporter::Import(const tinygltf::Material& gltfMat, const tinygltf::Model& model, const std::string& sceneName, const std::string& outputFile, int materialIndex, const std::string& basePath) {
    ModuleFileSystem* fs = app->getFileSystem();
    const auto& pbr = gltfMat.pbrMetallicRoughness;

    MaterialHeader header;
    std::string texturePath;

    if (pbr.baseColorTexture.index >= 0 && pbr.baseColorTexture.index < (int)model.textures.size()) {
        const auto& tex = model.textures[pbr.baseColorTexture.index];
        if (tex.source >= 0 && tex.source < (int)model.images.size()) {
            const std::string& uri = model.images[tex.source].uri;
            if (!uri.empty()) {
                std::string ddsPath = fs->GetLibraryPath() + "Materials/" + sceneName + "/" + TextureImporter::GetTextureName(uri.c_str()) + ".dds";
                if (fs->Exists(ddsPath.c_str()) || TextureImporter::Import((basePath + uri).c_str(), ddsPath)) {
                    header.hasTexture = 1;
                    texturePath = ddsPath;
                }
                else {
                    LOG("MaterialImporter: Failed to import texture %s", (basePath + uri).c_str());
                }
            }
        }
    }

    header.texturePathLength = (uint32_t)texturePath.size();
    return Save(header, texturePath, outputFile);
}

bool MaterialImporter::Load(const std::string& file, std::unique_ptr<Material>& outMaterial) {
    MaterialHeader header;
    std::vector<char> rawBuffer;
    if (!ImporterUtils::LoadBuffer(file, header, rawBuffer)) return false;
    if (!ImporterUtils::ValidateHeader(header, 0x4D415452)) { LOG("MaterialImporter: Invalid file format"); return false; }

    std::string texturePath;
    if (header.hasTexture && header.texturePathLength > 0) texturePath.assign(rawBuffer.data() + sizeof(MaterialHeader), header.texturePathLength);

    outMaterial = std::make_unique<Material>();

    if (!texturePath.empty()) {
        ComPtr<ID3D12Resource> texture;
        D3D12_GPU_DESCRIPTOR_HANDLE srv;
        if (TextureImporter::Load(texturePath, texture, srv)) outMaterial->setBaseColorTexture(texture, srv);
        else LOG("MaterialImporter: Failed to load texture %s", texturePath.c_str());
    }

    return true;
}

bool MaterialImporter::Save(const MaterialHeader& header, const std::string& texturePath, const std::string& file) {
    std::vector<char> payload(texturePath.begin(), texturePath.end());
    if (!ImporterUtils::SaveBuffer(file, header, payload)) { LOG("MaterialImporter: Failed to save %s", file.c_str()); return false; }
    return true;
}