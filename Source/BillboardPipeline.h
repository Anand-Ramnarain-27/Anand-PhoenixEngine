#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class BillboardPipeline {
public:
    static constexpr UINT SLOT_CB = 0;
    static constexpr UINT SLOT_TEXTURE = 1;
    static constexpr UINT SLOT_SAMPLER = 2;

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12PipelineState* getAdditivePSO() const { return m_additivePso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
    ComPtr<ID3D12PipelineState> m_additivePso;
};
