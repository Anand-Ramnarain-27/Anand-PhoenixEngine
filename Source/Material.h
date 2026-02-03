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

    struct PBRPhongMaterial
    {
        XMFLOAT4 diffuseColor = XMFLOAT4(1, 1, 1, 1);

        XMFLOAT3 F0 = XMFLOAT3(0.04f, 0.04f, 0.04f);

        float shininess = 32.0f;

        BOOL hasDiffuseTexture = FALSE;
        float padding[3] = {};
    };

public:
    Material();

    void load(const tinygltf::Material& gltfMat,
        const tinygltf::Model& model,
        const char* basePath);

    const BasicMaterial& getBasic() const { return basicData; }
    const PhongMaterial& getPhong() const { return phongData; }
    const PBRPhongMaterial& getPBRPhong() const { return pbrPhongData; }

    ComPtr<ID3D12Resource> getTexture() const { return texture; }
    D3D12_GPU_DESCRIPTOR_HANDLE getTextureGPUHandle() const { return gpuHandle; }
    bool hasTexture() const { return textureLoaded; }
    const std::string& getName() const { return name; }

    bool isDirty() const { return dirty; }
    void clearDirty() { dirty = false; }

    void setPhong(const PhongMaterial& phong)
    {
        phongData = phong;
        dirty = true;
    }

private:
    BasicMaterial basicData;
    PhongMaterial phongData;
    PBRPhongMaterial pbrPhongData;

    ComPtr<ID3D12Resource> texture;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
    std::string name;
    bool textureLoaded = false;
    bool dirty = false;
};
