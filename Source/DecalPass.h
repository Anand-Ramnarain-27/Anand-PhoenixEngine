#pragma once
// Projects ComponentDecal textures into the G-buffer before lighting.

#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class DecalPipeline {
public:
    static constexpr UINT SLOT_CB = 0;
    static constexpr UINT SLOT_DEPTH = 1;
    static constexpr UINT SLOT_ALBEDO = 2;
    static constexpr UINT SLOT_SAMPLER = 3;

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <unordered_map>
using Microsoft::WRL::ComPtr;

class GBufferPass;

// Everything up to texturePath is the shader's CbDecal (Decal.hlsli) and is copied as-is.
struct DecalInstance {
    Matrix mvp;
    Matrix invModel;
    Matrix invViewProj;
    Vector4 colourOpacity;
    Vector4 emissiveAlbedoMix;   // rgb = emissive strength per channel (x colour), w = how much it tints albedo (0..1)
    std::string texturePath;
};
static constexpr size_t kDecalCBBytes = sizeof(Matrix) * 3 + sizeof(Vector4) * 2;

/// Draws each decal's box and writes albedo / emissive where it covers G-buffer surfaces.
class DecalPass {
public:
    static constexpr UINT MAX_DECALS = 64;
    // The editor renders the Scene and Game views in one frame, and up to FRAMES_IN_FLIGHT frames can be queued,
    // so each (frame, view) gets its own slice of the CB ring. With one shared slice the second view would
    // overwrite the first view's matrices before the GPU reads them, projecting its decals with the other camera.
    static constexpr UINT MAX_VIEWS_PER_FRAME = 2;
    static constexpr UINT SLOTS_PER_FRAME = MAX_DECALS * MAX_VIEWS_PER_FRAME;

    bool init(ID3D12Device* device);

    void beginFrame() { m_frameCBCursor = 0; }

    void render(ID3D12GraphicsCommandList* cmd,
                GBufferPass& gbufferPass,
                const std::vector<DecalInstance>& decals,
                uint32_t width, uint32_t height);

private:
    bool createUnitBox(ID3D12Device* device);
    bool createUploadBuffers(ID3D12Device* device);
    bool createFallbackTexture(ID3D12Device* device, ID3D12GraphicsCommandList* cmd,
                                ComPtr<ID3D12Resource>& texUpload);

    DecalPipeline m_pipeline;

    ComPtr<ID3D12Resource> m_vb;
    ComPtr<ID3D12Resource> m_ib;
    D3D12_VERTEX_BUFFER_VIEW m_vbv = {};
    D3D12_INDEX_BUFFER_VIEW m_ibv = {};
    UINT m_indexCount = 0;

    ComPtr<ID3D12Resource> m_cbRing;
    void* m_cbMapped = nullptr;
    UINT m_frameCBCursor = 0;   // slots used this frame (all views); reset by beginFrame()

    ComPtr<ID3D12Resource> m_fallbackTex;
    ShaderTableDesc m_fallbackSRV;

    D3D12_GPU_DESCRIPTOR_HANDLE getOrLoadTexture(const std::string& path);
    struct CachedTexture {
        ComPtr<ID3D12Resource> resource;
        ShaderTableDesc srv;
    };
    std::unordered_map<std::string, CachedTexture> m_textureCache;
};

