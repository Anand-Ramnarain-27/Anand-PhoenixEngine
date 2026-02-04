#pragma once
#include "Globals.h"
#include "ShaderTableDesc.h"

namespace tinygltf
{
    class Model;
    struct Material;
}

class Material
{
public:
    struct Data
    {
        XMFLOAT4 baseColor = XMFLOAT4(1, 1, 1, 1);
        XMFLOAT3 emissive = XMFLOAT3(0, 0, 0);
        float metallic = 0.0f;
        float roughness = 1.0f;
        float occlusion = 1.0f;
        BOOL hasBaseColorTex = FALSE;
        BOOL hasMetallicRoughnessTex = FALSE;
        BOOL hasNormalTex = FALSE;
        BOOL hasEmissiveTex = FALSE;
        BOOL hasOcclusionTex = FALSE;
        float padding[3] = {}; // For 16-byte alignment
    };

public:
    Material() = default; // Use default constructor
    ~Material() = default;

    // Enable move, disable copy
    Material(Material&&) = default;
    Material& operator=(Material&&) = default;
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;

    bool load(const tinygltf::Material& gltfMat,
        const tinygltf::Model& model,
        const char* basePath);

    const Data& getData() const { return m_data; }
    const ShaderTableDesc& getShaderTable() const { return m_shaderTable; }
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle() const { return m_gpuHandle; }
    const std::string& getName() const { return m_name; }

    bool hasTexture() const { return m_hasTexture; }

private:
    bool loadTexture(int texIndex, const tinygltf::Model& model,
        const char* basePath, bool sRGB, ComPtr<ID3D12Resource>& outTex);

private:
    Data m_data;
    std::string m_name;
    ShaderTableDesc m_shaderTable;
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuHandle = {};

    ComPtr<ID3D12Resource> m_baseColorTex;
    ComPtr<ID3D12Resource> m_metallicRoughnessTex;
    ComPtr<ID3D12Resource> m_normalTex;
    ComPtr<ID3D12Resource> m_emissiveTex;
    ComPtr<ID3D12Resource> m_occlusionTex;

    bool m_hasTexture = false;
};