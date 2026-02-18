#pragma once

#include "ModuleD3D12.h"
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class Material
{
public:
   struct Data
    {
        Vector4 baseColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        float metallic = 0.0f;
        float roughness = 0.5f;
        uint32_t hasBaseColorTexture = 0;
        uint32_t samplerIndex = 0;
    };

public:
    Material();
    ~Material();
    
    const Data& getData() const { return m_data; }
    Data& getData() { return m_data; }

    void setBaseColorTexture(ComPtr<ID3D12Resource> texture, D3D12_GPU_DESCRIPTOR_HANDLE srvHandle);
    bool hasTexture() const { return m_hasTexture; }
    D3D12_GPU_DESCRIPTOR_HANDLE getTextureGPUHandle() const { return m_textureSRV; }

private:
    Data m_data;
    ComPtr<ID3D12Resource> m_baseColorTexture;
    D3D12_GPU_DESCRIPTOR_HANDLE m_textureSRV = {};
    bool m_hasTexture = false;
};