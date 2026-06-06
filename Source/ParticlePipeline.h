#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

// Root-signatures + PSOs for GPU particle simulation and rendering.
//
// Compute (update) root signature:
//   slot 0  b0 — CbParticle (gravity, dt, colour range, max count)
//   slot 1  u0 — RWStructuredBuffer<Particle>  (particle state UAV)
//   slot 2  u1 — RWBuffer<uint>                (dead-count atomic UAV)
//
// Graphics (render) root signature:
//   slot 0  b0 — CbParticleRender (viewproj, camera axes, half-size)
//   slot 1  t0 — StructuredBuffer<Particle>    (particle SRV — compute writes, VS reads)
//   slot 2  t1 — Texture2D                     (particle sprite)
//   slot 3  s0-s3 — samplers
class ParticlePipeline {
public:
    // Compute slots
    static constexpr UINT CS_SLOT_CB          = 0;
    static constexpr UINT CS_SLOT_PARTICLES   = 1;
    static constexpr UINT CS_SLOT_DEAD_COUNT  = 2;

    // Graphics slots
    static constexpr UINT GFX_SLOT_CB         = 0;
    static constexpr UINT GFX_SLOT_PARTICLES  = 1;
    static constexpr UINT GFX_SLOT_SPRITE     = 2;
    static constexpr UINT GFX_SLOT_SAMPLER    = 3;

    bool init(ID3D12Device* device, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat);

    ID3D12RootSignature* getComputeRootSig()  const { return m_csRootSig.Get(); }
    ID3D12RootSignature* getGraphicsRootSig() const { return m_gfxRootSig.Get(); }
    ID3D12PipelineState* getUpdatePSO()       const { return m_updatePSO.Get(); }
    ID3D12PipelineState* getRenderPSO()       const { return m_renderPSO.Get(); }

private:
    bool createComputeRootSig(ID3D12Device* device);
    bool createGraphicsRootSig(ID3D12Device* device);
    bool createUpdatePSO(ID3D12Device* device);
    bool createRenderPSO(ID3D12Device* device, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat);

    ComPtr<ID3D12RootSignature> m_csRootSig;
    ComPtr<ID3D12RootSignature> m_gfxRootSig;
    ComPtr<ID3D12PipelineState> m_updatePSO;
    ComPtr<ID3D12PipelineState> m_renderPSO;
};
