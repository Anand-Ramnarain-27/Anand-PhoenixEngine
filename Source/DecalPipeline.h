#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

// Root-signature + graphics PSO for the deferred decal pass.
// Renders unit boxes that project textures onto the G-Buffer.
class DecalPipeline {
public:
    static constexpr UINT SLOT_CB        = 0;  // b0 – CbDecal (VS+PS)
    static constexpr UINT SLOT_DEPTH     = 1;  // t0 – depth SRV (PS)
    static constexpr UINT SLOT_ALBEDO    = 2;  // t1 – decal albedo texture (PS)
    static constexpr UINT SLOT_SAMPLER   = 3;  // s0-s3 – samplers (PS)

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO()     const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};
