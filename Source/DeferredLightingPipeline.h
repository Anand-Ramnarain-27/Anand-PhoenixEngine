#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class DeferredLightingPipeline {
public:
    static constexpr UINT SLOT_PERFRAME_CB = 0; // b0, pixel  — CbPerFrame
    static constexpr UINT SLOT_DIR_LIGHTS = 1; // t0, pixel  — StructuredBuffer<DirectionalLight>
    static constexpr UINT SLOT_POINT_LIGHTS = 2; // t1, pixel  — StructuredBuffer<PointLight>
    static constexpr UINT SLOT_SPOT_LIGHTS = 3; // t2, pixel  — StructuredBuffer<SpotLight>
    static constexpr UINT SLOT_IRRADIANCE = 4; // t3, pixel  — TextureCube irradiance
    static constexpr UINT SLOT_PREFILTER = 5; // t4, pixel  — TextureCube prefiltered
    static constexpr UINT SLOT_BRDF_LUT = 6; // t5, pixel  — Texture2D BRDF LUT
    static constexpr UINT SLOT_GBUF_ALBEDO = 7; // t6, pixel  — GBuffer albedo
    static constexpr UINT SLOT_GBUF_NORMAL = 8; // t7, pixel  — GBuffer normalMetalRough
    static constexpr UINT SLOT_GBUF_EMISSIVE = 9; // t8, pixel  — GBuffer emissiveAO
    static constexpr UINT SLOT_GBUF_DEPTH = 10;      // t9,  pixel  — GBuffer depth (R32F)
    static constexpr UINT SLOT_POINT_INDICES = 11;  // t10, pixel  — per-tile point light indices
    static constexpr UINT SLOT_SPOT_INDICES  = 12;  // t11, pixel  — per-tile spot  light indices
    static constexpr UINT SLOT_SAMPLER = 13;        // s0-s3, pixel

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};
