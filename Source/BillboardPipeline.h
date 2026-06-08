#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

// Root-signature + graphics PSO for the billboard forward pass.
// Renders camera-facing quads generated entirely in the vertex shader
// (SV_VertexID, no vertex/index buffers) with alpha blending against the
// scene colour target, sharing the GBuffer depth read-only.
class BillboardPipeline {
public:
    static constexpr UINT SLOT_CB      = 0;  // b0 – CbBillboard (VS+PS)
    static constexpr UINT SLOT_TEXTURE = 1;  // t0 – sprite sheet (PS)
    static constexpr UINT SLOT_SAMPLER = 2;  // s0-s3 – samplers (PS)

    bool init(ID3D12Device* device);

    // Alpha: src*srcAlpha + dst*(1-srcAlpha) — standard transparency.
    // Additive: src + dst — fire / glow / sparks (lecture: "Set additive blending").
    ID3D12PipelineState* getPSO()         const { return m_pso.Get(); }
    ID3D12PipelineState* getAdditivePSO() const { return m_additivePso.Get(); }
    ID3D12RootSignature* getRootSig()     const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
    ComPtr<ID3D12PipelineState> m_additivePso;
};
