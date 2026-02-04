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
        XMFLOAT4 baseColour;
        BOOL hasColourTexture;
    };

public:
    Material();

    Material(Material&& other) noexcept;
    Material& operator=(Material&& other) noexcept;

    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;

    ~Material();

    void load(const tinygltf::Material& gltfMaterial, const tinygltf::Model& model, const char* basePath);

    const Data& getData() const { return m_data; }
    ComPtr<ID3D12Resource> getTexture() const { return m_texture; }
    const char* getName() const { return m_name.c_str(); }

    D3D12_GPU_DESCRIPTOR_HANDLE getTextureGPUHandle() const { return m_textureGPUHandle; }
    bool hasTexture() const { return m_hasTexture; }

    const ShaderTableDesc& getShaderTable() const { return m_shaderTable; }

private:
    Data m_data;
    ComPtr<ID3D12Resource> m_texture;
    ShaderTableDesc m_shaderTable;
    std::string m_name;
    bool m_hasTexture = false;
    D3D12_GPU_DESCRIPTOR_HANDLE m_textureGPUHandle = { 0 };
};