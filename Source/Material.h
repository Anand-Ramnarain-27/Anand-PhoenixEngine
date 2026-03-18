#pragma once

#include "Globals.h"
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class Material {
public:
    // NOTE: This layout must match MaterialCB in MeshPS.hlsl exactly.
    // HLSL cbuffer packing rules require float3 to start on a 16-byte boundary.
    // After hasMetalRoughMap (offset 52), there is an implicit 4-byte pad before
    // emissiveFactor (offset 64). The explicit _pad field below matches that gap.
    struct Data {
        Vector4 baseColor = Vector4(1.f, 1.f, 1.f, 1.f); // offset  0
        float metallic = 0.f;                              // offset 16
        float roughness = 0.5f;                            // offset 20
        float normalStrength = 1.f;                        // offset 24
        float aoStrength = 1.f;                            // offset 28

        float exposure = 1.f;                              // offset 32

        uint32_t hasBaseColorTexture = 0;                  // offset 36
        uint32_t hasNormalMap = 0;                         // offset 40
        uint32_t hasAOMap = 0;                             // offset 44
        uint32_t hasEmissiveMap = 0;                       // offset 48
        uint32_t hasMetalRoughMap = 0;                     // offset 52

        uint32_t _pad = 0;                                 // offset 56 - matches HLSL implicit pad

        Vector3 emissiveFactor = Vector3(1.f, 1.f, 1.f);  // offset 64
        uint32_t samplerIndex = 0;                         // offset 76
    };

    Material() = default;
    ~Material() = default;

    const Data& getData() const { return m_data; }
    Data& getData() { return m_data; }

    void setBaseColorTexture(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv);
    void setNormalMap(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv);
    void setAOMap(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv);
    void setEmissiveMap(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv);
    void setMetalRoughMap(ComPtr<ID3D12Resource> tex, D3D12_GPU_DESCRIPTOR_HANDLE srv);

    bool hasTexture() const { return m_hasBaseColor; }
    bool hasNormalMap() const { return m_hasNormal; }
    bool hasAOMap() const { return m_hasAO; }
    bool hasEmissive() const { return m_hasEmissive; }
    bool hasMetalRoughMap() const { return m_hasMetalRough; }

    D3D12_GPU_DESCRIPTOR_HANDLE getTextureGPUHandle() const { return m_baseColorSRV; }
    D3D12_GPU_DESCRIPTOR_HANDLE getNormalMapGPUHandle() const { return m_normalSRV; }
    D3D12_GPU_DESCRIPTOR_HANDLE getAOMapGPUHandle() const { return m_aoSRV; }
    D3D12_GPU_DESCRIPTOR_HANDLE getEmissiveGPUHandle() const { return m_emissiveSRV; }
    D3D12_GPU_DESCRIPTOR_HANDLE getMetalRoughGPUHandle() const { return m_metalRoughSRV; }

private:
    Data m_data;

    ComPtr<ID3D12Resource> m_baseColorTex;
    D3D12_GPU_DESCRIPTOR_HANDLE m_baseColorSRV = {};
    bool m_hasBaseColor = false;

    ComPtr<ID3D12Resource> m_normalTex;
    D3D12_GPU_DESCRIPTOR_HANDLE m_normalSRV = {};
    bool m_hasNormal = false;

    ComPtr<ID3D12Resource> m_aoTex;
    D3D12_GPU_DESCRIPTOR_HANDLE m_aoSRV = {};
    bool m_hasAO = false;

    ComPtr<ID3D12Resource> m_emissiveTex;
    D3D12_GPU_DESCRIPTOR_HANDLE m_emissiveSRV = {};
    bool m_hasEmissive = false;

    ComPtr<ID3D12Resource> m_metalRoughTex;
    D3D12_GPU_DESCRIPTOR_HANDLE m_metalRoughSRV = {};
    bool m_hasMetalRough = false;
};