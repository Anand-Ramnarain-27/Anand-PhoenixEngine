#include "Globals.h"
#include "Material.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "tiny_gltf.h"

Material::Material()
    : uid(GenerateUID()){
}
bool Material::load(const tinygltf::Material& gltfMaterial,
    const tinygltf::Model& model,
    const char* basePath)
{
    m_name = gltfMaterial.name.empty() ? "Material" : gltfMaterial.name;

    m_descriptorTable.reset();
    m_hasTexture = false;
    m_texture.Reset();

    const auto& pbr = gltfMaterial.pbrMetallicRoughness;
    if (pbr.baseColorFactor.size() >= 4)
    {
        m_data.baseColour = XMFLOAT4(
            static_cast<float>(pbr.baseColorFactor[0]),
            static_cast<float>(pbr.baseColorFactor[1]),
            static_cast<float>(pbr.baseColorFactor[2]),
            static_cast<float>(pbr.baseColorFactor[3])
        );

        m_pbrData.diffuseColour = XMFLOAT3(
            static_cast<float>(pbr.baseColorFactor[0]),
            static_cast<float>(pbr.baseColorFactor[1]),
            static_cast<float>(pbr.baseColorFactor[2])
        );
    }

    if (pbr.baseColorTexture.index >= 0)
    {
        m_hasTexture = loadTextureFromGltf(model, pbr.baseColorTexture.index, basePath);
        m_data.hasColourTexture = m_hasTexture ? TRUE : FALSE;
        m_pbrData.hasDiffuseTex = m_hasTexture ? TRUE : FALSE;
    }

    if (!m_hasTexture)
    {
        createNullDescriptor();
        m_data.hasColourTexture = FALSE;
        m_pbrData.hasDiffuseTex = FALSE;
    }

    return m_descriptorTable.isValid();
}

bool Material::loadPBRPhong(const tinygltf::Material& gltfMaterial,
    const tinygltf::Model& model,
    const char* basePath)
{
    if (!load(gltfMaterial, model, basePath))
        return false;

    const auto& pbr = gltfMaterial.pbrMetallicRoughness;

    m_pbrData.metallicFactor = static_cast<float>(pbr.metallicFactor);
    m_pbrData.roughnessFactor = static_cast<float>(pbr.roughnessFactor);

    m_pbrData.specularColour = XMFLOAT3(0.04f, 0.04f, 0.04f);

    m_pbrData.shininess = 100.0f * (1.0f - m_pbrData.roughnessFactor);

    return true;
}

void Material::setPBRPhongData(const PBRPhongMaterialData& data)
{
    m_pbrData = data;

    m_data.baseColour = XMFLOAT4(data.diffuseColour.x, data.diffuseColour.y, data.diffuseColour.z, 1.0f);
    m_data.hasColourTexture = data.hasDiffuseTex;
}

bool Material::loadTextureFromGltf(const tinygltf::Model& model, int textureIndex, const char* basePath)
{
    if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size()))
        return false;

    const auto& texture = model.textures[textureIndex];
    if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size()))
        return false;

    const auto& image = model.images[texture.source];
    if (image.uri.empty())
        return false;

    ModuleResources* resources = app->getResources();
    std::string fullPath = std::string(basePath) + image.uri;

    ComPtr<ID3D12Resource> textureResource = resources->createTextureFromFile(fullPath, true);
    if (!textureResource)
        return false;

    m_texture.Swap(textureResource);

    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    if (!descriptors)
        return false;

    m_descriptorTable = descriptors->allocTable(m_name.c_str());
    if (!m_descriptorTable.isValid())
        return false;

    m_descriptorTable.createTexture2DSRV(m_texture.Get(), 0);
    return true;
}

void Material::createNullDescriptor()
{
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    if (!descriptors)
        return;

    std::string tableName = m_name + "_Null";
    m_descriptorTable = descriptors->allocTable(tableName.c_str());
    if (m_descriptorTable.isValid())
    {
        m_descriptorTable.createNullSRV(0);
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE Material::getTextureGPUHandle() const
{
    return m_descriptorTable.getGPUHandle(0);
}