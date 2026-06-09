#pragma once
#include "DecalPipeline.h"
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
using Microsoft::WRL::ComPtr;

class GBufferPass;

// Per-decal constant buffer data uploaded to the GPU each frame.
struct DecalInstance {
    Matrix mvp;
    Matrix invModel;
    Matrix invViewProj;
};

// Deferred decal rendering pass.
// Runs after the GBuffer geometry pass but before the deferred lighting pass.
// For each decal it renders a unit box and the PS writes the decal texture
// into the G-Buffer albedo render target.
class DecalPass {
public:
    static constexpr UINT MAX_DECALS = 64;

    bool init(ID3D12Device* device);

    // Render all decals.
    // G-Buffer color RTs must be in SRV state on entry (endGeomPass was called).
    // They will be transitioned RTV→SRV by this call.
    void render(ID3D12GraphicsCommandList* cmd,
                GBufferPass& gbufferPass,
                const std::vector<DecalInstance>& decals,
                uint32_t width, uint32_t height);

private:
    bool createUnitBox(ID3D12Device* device);
    bool createUploadBuffers(ID3D12Device* device);
    bool createFallbackTexture(ID3D12Device* device);

    DecalPipeline m_pipeline;

    // Unit box geometry
    ComPtr<ID3D12Resource> m_vb;
    ComPtr<ID3D12Resource> m_ib;
    D3D12_VERTEX_BUFFER_VIEW m_vbv = {};
    D3D12_INDEX_BUFFER_VIEW m_ibv = {};
    UINT m_indexCount = 0;

    // Per-decal CB ring (upload heap, MAX_DECALS slots)
    ComPtr<ID3D12Resource> m_cbRing;
    void* m_cbMapped = nullptr;

    // Fallback 1×1 white texture used when a decal has no texture assigned
    ComPtr<ID3D12Resource> m_fallbackTex;
    ShaderTableDesc m_fallbackSRV;
};
