#pragma once

#include "Globals.h"
#include "ShaderTableDesc.h"
#include <string>
#include <vector>
#include <wrl/client.h>
#include "UID.h"

using Microsoft::WRL::ComPtr;

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
        XMFLOAT4 baseColour = { 1,1,1,1 };
        BOOL hasColourTexture = FALSE;
        float padding[11] = {};
    };

public:
    Material();
    ~Material() = default;

    // ?? Phase 4 runtime loading
    bool loadFromBinary(const char* path);

    const Data& getData() const { return m_data; }
    const PBRPhongMaterialData& getPBRPhongData() const { return m_pbrData; }

    bool hasTexture() const { return m_hasTexture; }
    D3D12_GPU_DESCRIPTOR_HANDLE getTextureGPUHandle() const;
    const ShaderTableDesc& getDescriptorTable() const { return m_descriptorTable; }

    UID GetUID() const { return uid; }

private:
    void createNullDescriptor();

private:
    Data m_data;
    PBRPhongMaterialData m_pbrData;

    ComPtr<ID3D12Resource> m_texture;
    ShaderTableDesc m_descriptorTable;

    bool m_hasTexture = false;
    UID uid;
};
