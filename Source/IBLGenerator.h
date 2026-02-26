#pragma once
// IBLGenerator.h
// Performs all three offline IBL GPU bakes from a loaded source cubemap.
// Call generate() once after loading; the caller must flush + wait the GPU.
//
//  Bake 1 – Irradiance cubemap    32x32  R16G16B16A16_FLOAT  (diffuse ambient)
//  Bake 2 – Pre-filtered env map  128x128 x 5 mips           (specular)
//  Bake 3 – BRDF integration LUT  512x512 R16G16_FLOAT       (fa / fb table)

#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;

class EnvironmentMap;

class IBLGenerator
{
public:
    // Bakes irradiance, pre-filter and BRDF LUT into env.
    // `cmd` must be open on a DIRECT queue; caller flushes afterward.
    bool generate(ID3D12Device* device,
        ID3D12GraphicsCommandList* cmd,
        EnvironmentMap& env);

private:
    // Constant buffer sent to every cubemap-convolution draw (one per face/mip).
    // Matches the layout declared in CubemapConvVS.hlsl / PrefilterEnvMapPS.hlsl.
    struct alignas(256) FaceCB
    {
        DirectX::XMFLOAT4X4 vp;       // view * proj for this face
        int   flipX;                   // coordinate-system correction flags
        int   flipZ;
        float roughness;               // [0,1] – used only by prefilter PS
        float _pad;
    };

    // ---- Resource creation ----------------------------------------------
    bool createCubemapResource(ID3D12Device* device, uint32_t size, uint32_t mips,
        DXGI_FORMAT fmt, const wchar_t* name,
        ComPtr<ID3D12Resource>& out);

    bool create2DResource(ID3D12Device* device, uint32_t size,
        DXGI_FORMAT fmt, const wchar_t* name,
        ComPtr<ID3D12Resource>& out);

    // ---- Pipeline creation ----------------------------------------------
    // Shared root signature layout for the two cubemap-conv passes:
    //   param 0 = CBV  b0 (FaceCB)
    //   param 1 = descriptor table t0 (source cubemap SRV)
    //   static sampler s0 = linear-wrap
    bool createConvPipeline(ID3D12Device* device,
        const wchar_t* psCsoPath, DXGI_FORMAT rtvFmt,
        ComPtr<ID3D12RootSignature>& outRS,
        ComPtr<ID3D12PipelineState>& outPSO);

    // BRDF LUT root signature: empty (uv from full-screen triangle encodes inputs)
    bool createBRDFPipeline(ID3D12Device* device,
        ComPtr<ID3D12RootSignature>& outRS,
        ComPtr<ID3D12PipelineState>& outPSO);

    // ---- Per-face render ------------------------------------------------
    void renderCubeFace(ID3D12Device* device, ID3D12GraphicsCommandList* cmd,
        ID3D12Resource* target, uint32_t faceIndex,
        uint32_t mipLevel, uint32_t totalMips,
        uint32_t baseFaceSize, float roughness,
        ID3D12RootSignature* rs, ID3D12PipelineState* pso,
        D3D12_GPU_DESCRIPTOR_HANDLE sourceSRV,
        DXGI_FORMAT rtvFmt);

    // ---- Shared state ---------------------------------------------------
    bool ensureGeometry(ID3D12Device* device);
    bool ensureFaceCB(ID3D12Device* device);

    ComPtr<ID3D12Resource>   m_cubeVB;
    D3D12_VERTEX_BUFFER_VIEW m_vbView = {};
    bool                     m_geometryReady = false;

    ComPtr<ID3D12Resource> m_faceCB;      // persistently-mapped upload buffer
    FaceCB* m_faceCBPtr = nullptr;

    // Pipelines stored as members so they stay alive through ExecuteCommandLists
    // + flush(). Call releasePipelines() after the GPU flush to free them.
    ComPtr<ID3D12RootSignature> m_irradianceRS, m_prefilterRS, m_brdfRS;
    ComPtr<ID3D12PipelineState> m_irradiancePSO, m_prefilterPSO, m_brdfPSO;

public:
    // Call after flush() to release the temporary bake pipelines.
    void releasePipelines()
    {
        m_irradianceRS.Reset(); m_irradiancePSO.Reset();
        m_prefilterRS.Reset();  m_prefilterPSO.Reset();
        m_brdfRS.Reset();       m_brdfPSO.Reset();
    }
};