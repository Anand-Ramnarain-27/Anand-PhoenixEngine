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
    struct BasicMaterial
    {
        XMFLOAT4 color = XMFLOAT4(1, 1, 1, 1);
        BOOL hasColorTexture = FALSE;
        float padding[3] = {};
    };

    struct PhongMaterial
    {
        XMFLOAT4 diffuseColor = XMFLOAT4(1, 1, 1, 1);
        float Kd = 0.8f;
        float Ks = 0.2f;
        float shininess = 32.f;
        BOOL hasDiffuseTexture = FALSE;
        float padding[3] = {};
    };

public:
    Material();

    // Rule of five - enable move semantics
    Material(Material&& other) noexcept = default;
    Material& operator=(Material&& other) noexcept = default;

    // Disable copying
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;

    void load(const tinygltf::Material& gltfMat,
        const tinygltf::Model& model,
        const char* basePath);

    const BasicMaterial& getBasic() const { return basicData; }
    const PhongMaterial& getPhong() const { return phongData; }

    ComPtr<ID3D12Resource> getTexture() const { return texture; }
    D3D12_GPU_DESCRIPTOR_HANDLE getTextureGPUHandle() const { return gpuHandle; }
    bool hasTexture() const { return textureLoaded; }
    const std::string& getName() const { return name; }

    // NEW: Get the shader table directly if needed
    const ShaderTableDesc& getShaderTable() const { return shaderTable; }

private:
    BasicMaterial basicData;
    PhongMaterial phongData;

    ComPtr<ID3D12Resource> texture;
    ShaderTableDesc shaderTable;  // NEW: Store the descriptor table
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
    std::string name;
    bool textureLoaded = false;
};