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

class Material
{
public:
    struct Data
    {
        XMFLOAT4 baseColour = { 1.0f, 1.0f, 1.0f, 1.0f };
        BOOL hasColourTexture = FALSE;
        float metallicFactor = 0.0f;
        float roughnessFactor = 1.0f;
        float padding[2] = {}; 
    };

public:
    Material() = default;

    bool load(const tinygltf::Material& gltfMaterial, const tinygltf::Model& model, const char* basePath);

    const Data& getData() const { return m_data; }
    const ComPtr<ID3D12Resource>& getTexture() const { return m_texture; }
    const char* getName() const { return m_name.c_str(); }
    bool hasTexture() const { return m_hasTexture; }

    D3D12_GPU_DESCRIPTOR_HANDLE getTextureGPUHandle() const;
    const ShaderTableDesc& getDescriptorTable() const { return m_descriptorTable; }

private:
    bool loadTextureFromGltf(const tinygltf::Model& model, int textureIndex, const char* basePath);
    void createNullDescriptor();

    Data m_data;
    ComPtr<ID3D12Resource> m_texture;
    std::string m_name;
    ShaderTableDesc m_descriptorTable;
    bool m_hasTexture = false;
};