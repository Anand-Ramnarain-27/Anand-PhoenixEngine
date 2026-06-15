#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class ParticlePipeline {
public:
    static constexpr UINT GFX_SLOT_CB = 0;
    static constexpr UINT GFX_SLOT_PARTICLES = 1;
    static constexpr UINT GFX_SLOT_TEXTURE = 2;
    static constexpr UINT GFX_SLOT_SAMPLER = 3;

    static constexpr UINT CS_SLOT_CB = 0;
    static constexpr UINT CS_SLOT_INPUT = 1;
    static constexpr UINT CS_SLOT_OUTPUT = 2;

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12PipelineState* getAdditivePSO() const { return m_additivePso.Get(); }
    ID3D12PipelineState* getComputePSO() const { return m_computePso.Get(); }
    ID3D12RootSignature* getGfxRootSig() const { return m_gfxRootSig.Get(); }
    ID3D12RootSignature* getCsRootSig() const { return m_csRootSig.Get(); }

private:
    bool createGfxRootSignature(ID3D12Device* device);
    bool createCsRootSignature(ID3D12Device* device);
    bool createGraphicsPSOs(ID3D12Device* device);
    bool createComputePSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_gfxRootSig;
    ComPtr<ID3D12RootSignature> m_csRootSig;
    ComPtr<ID3D12PipelineState> m_pso;
    ComPtr<ID3D12PipelineState> m_additivePso;
    ComPtr<ID3D12PipelineState> m_computePso;
};
