#include "Globals.h"
#include "Material.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"

#include "tiny_gltf.h"

// REMOVED: Material::Material() {} - Using default constructor

bool Material::loadTexture(int texIndex, const tinygltf::Model& model,
    const char* basePath, bool sRGB, ComPtr<ID3D12Resource>& outTex)
{
    if (texIndex < 0 || texIndex >= int(model.textures.size()))
        return false;

    const auto& tex = model.textures[texIndex];
    if (tex.source < 0 || tex.source >= int(model.images.size()))
        return false;

    const auto& img = model.images[tex.source];
    if (img.uri.empty())
        return false;

    ModuleResources* res = app->getResources();
    std::string fullPath = std::string(basePath) + img.uri;
    outTex = res->createTextureFromFile(fullPath, sRGB);
    return outTex != nullptr;
}

bool Material::load(const tinygltf::Material& gltfMat,
    const tinygltf::Model& model,
    const char* basePath)
{
    m_name = gltfMat.name.empty() ? "Unnamed" : gltfMat.name;

    const auto& pbr = gltfMat.pbrMetallicRoughness;

    // Load base color
    if (pbr.baseColorFactor.size() >= 4)
    {
        m_data.baseColor = XMFLOAT4{
            float(pbr.baseColorFactor[0]),
            float(pbr.baseColorFactor[1]),
            float(pbr.baseColorFactor[2]),
            float(pbr.baseColorFactor[3])
        };
    }

    m_data.metallic = float(pbr.metallicFactor);
    m_data.roughness = float(pbr.roughnessFactor);

    // Load emissive factor
    if (gltfMat.emissiveFactor.size() >= 3)
    {
        m_data.emissive = XMFLOAT3{
            float(gltfMat.emissiveFactor[0]),
            float(gltfMat.emissiveFactor[1]),
            float(gltfMat.emissiveFactor[2])
        };
    }

    // Load occlusion strength
    if (gltfMat.occlusionTexture.strength > 0)
    {
        m_data.occlusion = float(gltfMat.occlusionTexture.strength);
    }

    // Load all textures
    m_data.hasBaseColorTex = loadTexture(pbr.baseColorTexture.index, model, basePath, true, m_baseColorTex);
    m_data.hasMetallicRoughnessTex = loadTexture(pbr.metallicRoughnessTexture.index, model, basePath, false, m_metallicRoughnessTex);
    m_data.hasNormalTex = loadTexture(gltfMat.normalTexture.index, model, basePath, false, m_normalTex);
    m_data.hasEmissiveTex = loadTexture(gltfMat.emissiveTexture.index, model, basePath, true, m_emissiveTex);
    m_data.hasOcclusionTex = loadTexture(gltfMat.occlusionTexture.index, model, basePath, false, m_occlusionTex);

    m_hasTexture = m_data.hasBaseColorTex || m_data.hasMetallicRoughnessTex ||
        m_data.hasNormalTex || m_data.hasEmissiveTex || m_data.hasOcclusionTex;

    // Create descriptor table
    ModuleShaderDescriptors* desc = app->getShaderDescriptors();
    if (!desc)
        return false;

    m_shaderTable = desc->createTable();
    if (!m_shaderTable)
        return false;

    // Bind textures to slots (consistent ordering)
    if (m_data.hasBaseColorTex)
        m_shaderTable.createTexture2DSRV(m_baseColorTex.Get(), DXGI_FORMAT_UNKNOWN, 0, 0);
    else
        m_shaderTable.createNullSRV(D3D12_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

    if (m_data.hasMetallicRoughnessTex)
        m_shaderTable.createTexture2DSRV(m_metallicRoughnessTex.Get(), DXGI_FORMAT_UNKNOWN, 1, 1);
    else
        m_shaderTable.createNullSRV(D3D12_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM, 1);

    if (m_data.hasNormalTex)
        m_shaderTable.createTexture2DSRV(m_normalTex.Get(), DXGI_FORMAT_UNKNOWN, 2, 2);
    else
        m_shaderTable.createNullSRV(D3D12_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM, 2);

    if (m_data.hasEmissiveTex)
        m_shaderTable.createTexture2DSRV(m_emissiveTex.Get(), DXGI_FORMAT_UNKNOWN, 3, 3);
    else
        m_shaderTable.createNullSRV(D3D12_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM, 3);

    if (m_data.hasOcclusionTex)
        m_shaderTable.createTexture2DSRV(m_occlusionTex.Get(), DXGI_FORMAT_UNKNOWN, 4, 4);
    else
        m_shaderTable.createNullSRV(D3D12_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM, 4);

    m_gpuHandle = m_shaderTable.getGPUHandle();
    return true;
}