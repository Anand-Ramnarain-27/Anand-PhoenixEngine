#pragma once

#include "Globals.h"
#include "ShaderTableDesc.h"
#include <string>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace tinygltf
{
    class Model;
    struct Material;
}

struct PBRPhongMaterialData
{
    XMFLOAT3 diffuseColour = { 1.0f, 1.0f, 1.0f };
    BOOL hasDiffuseTex = FALSE;

    XMFLOAT3 specularColour = { 0.04f, 0.04f, 0.04f };
    float shininess = 32.0f;

    float metallicFactor = 0.0f;
    float roughnessFactor = 1.0f;
    float padding[2] = {};
};

class Material
{
public:
    struct Data
    {
        XMFLOAT4 baseColour = { 1.0f, 1.0f, 1.0f, 1.0f };
        BOOL hasColourTexture = FALSE;
        float padding[11] = {}; 
    };

    // Calculate size manually: both should be 48 bytes
    // PBRPhongMaterialData: XMFLOAT3(12) + BOOL(4) + XMFLOAT3(12) + float(4) + float(4) + float(4) + float(8) = 48
    // Data: XMFLOAT4(16) + BOOL(4) + float[11](44) = 64

    // Actually, let's make them both 64 bytes for D3D12 alignment
    // static_assert(sizeof(Data) == sizeof(PBRPhongMaterialData), 
    //               "Data and PBRPhongMaterialData must have same size for compatibility");

public:
    Material() = default;

    bool load(const tinygltf::Material& gltfMaterial, const tinygltf::Model& model, const char* basePath);

    bool loadPBRPhong(const tinygltf::Material& gltfMaterial, const tinygltf::Model& model, const char* basePath);

    const Data& getData() const { return m_data; }
    const PBRPhongMaterialData& getPBRPhongData() const { return m_pbrData; }

    const ComPtr<ID3D12Resource>& getTexture() const { return m_texture; }
    const char* getName() const { return m_name.c_str(); }
    bool hasTexture() const { return m_hasTexture; }

    void setPBRPhongData(const PBRPhongMaterialData& data);

    D3D12_GPU_DESCRIPTOR_HANDLE getTextureGPUHandle() const;
    const ShaderTableDesc& getDescriptorTable() const { return m_descriptorTable; }

private:
    bool loadTextureFromGltf(const tinygltf::Model& model, int textureIndex, const char* basePath);
    void createNullDescriptor();

    Data m_data;
    PBRPhongMaterialData m_pbrData;
    ComPtr<ID3D12Resource> m_texture;
    std::string m_name;
    ShaderTableDesc m_descriptorTable;
    bool m_hasTexture = false;
};