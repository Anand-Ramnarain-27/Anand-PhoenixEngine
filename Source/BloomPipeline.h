#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

// Root-signatures and PSOs for the three compute passes + one graphics composite.
//
// Compute passes (threshold, blur H, blur V) all share the same root signature layout:
//   slot 0  b0 — CbBloom constant buffer
//   slot 1  t0 — input texture SRV
//   slot 2  u0 — output texture UAV
//   slot 3  s0-s3 — samplers (for the composite PS only; bound in all for simplicity)
//
// Composite pass: fullscreen triangle with additive blending, one SRV input.
class BloomPipeline {
public:
    // Shared compute root-signature slots
    static constexpr UINT CS_SLOT_CB      = 0;
    static constexpr UINT CS_SLOT_INPUT   = 1;
    static constexpr UINT CS_SLOT_OUTPUT  = 2;
    static constexpr UINT CS_SLOT_SAMPLER = 3;

    // Composite graphics root-signature slots
    static constexpr UINT GFX_SLOT_BLOOM_SRV = 0;
    static constexpr UINT GFX_SLOT_SAMPLER   = 1;

    bool init(ID3D12Device* device, DXGI_FORMAT sceneRTFormat);

    ID3D12RootSignature* getComputeRootSig() const  { return m_computeRootSig.Get(); }
    ID3D12RootSignature* getCompositeRootSig() const{ return m_compositeRootSig.Get(); }

    ID3D12PipelineState* getThresholdPSO() const { return m_thresholdPSO.Get(); }
    ID3D12PipelineState* getBlurHPSO()    const { return m_blurHPSO.Get(); }
    ID3D12PipelineState* getBlurVPSO()    const { return m_blurVPSO.Get(); }
    ID3D12PipelineState* getCompositePSO()const { return m_compositePSO.Get(); }

private:
    bool createComputeRootSig(ID3D12Device* device);
    bool createCompositeRootSig(ID3D12Device* device);
    bool createComputePSOs(ID3D12Device* device);
    bool createCompositePSO(ID3D12Device* device, DXGI_FORMAT sceneRTFormat);

    ComPtr<ID3D12RootSignature> m_computeRootSig;
    ComPtr<ID3D12RootSignature> m_compositeRootSig;
    ComPtr<ID3D12PipelineState> m_thresholdPSO;
    ComPtr<ID3D12PipelineState> m_blurHPSO;
    ComPtr<ID3D12PipelineState> m_blurVPSO;
    ComPtr<ID3D12PipelineState> m_compositePSO;
};
