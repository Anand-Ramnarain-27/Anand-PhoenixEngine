#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class DeferredLightingPipeline {
public:
    static constexpr UINT SLOT_PERFRAME_CB = 0;
    static constexpr UINT SLOT_DIR_LIGHTS = 1;
    static constexpr UINT SLOT_POINT_LIGHTS = 2;
    static constexpr UINT SLOT_SPOT_LIGHTS = 3;
    static constexpr UINT SLOT_IRRADIANCE = 4;
    static constexpr UINT SLOT_PREFILTER = 5;
    static constexpr UINT SLOT_BRDF_LUT = 6;
    static constexpr UINT SLOT_GBUF_ALBEDO = 7;
    static constexpr UINT SLOT_GBUF_NORMAL = 8;
    static constexpr UINT SLOT_GBUF_EMISSIVE = 9;
    static constexpr UINT SLOT_GBUF_DEPTH = 10;
    static constexpr UINT SLOT_POINT_INDICES = 11;
    static constexpr UINT SLOT_SPOT_INDICES = 12;
    static constexpr UINT SLOT_SAMPLER = 13;

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};
