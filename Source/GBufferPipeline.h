#pragma once

#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class GBufferPipeline {
public:
    // Root parameter slots
    static constexpr UINT SLOT_MVP_CB       = 0; // b0, vertex  — CbMVP
    static constexpr UINT SLOT_INSTANCE_CB  = 1; // b1, all     — CbPerInstance
    static constexpr UINT SLOT_MAT_TEXTURES = 2; // t0-t4, pixel — 5 material textures
    static constexpr UINT SLOT_SAMPLER      = 3; // s0-s3, pixel — samplers

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO()     const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};
