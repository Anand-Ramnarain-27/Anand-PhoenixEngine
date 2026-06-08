#pragma once
#include "BillboardPipeline.h"
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <unordered_map>
using Microsoft::WRL::ComPtr;

class RenderTexture;
class GBufferPass;

// Per-billboard data uploaded to the GPU each frame.
// CenterHalfWidth.w / RightHalfHeight.w pack the half-extents alongside the
// quad axes so the whole instance fits in one constant buffer.
struct BillboardInstanceCB {
    Matrix viewProj;
    Vector4 centerHalfWidth;  // xyz = world-space centre, w = half width
    Vector4 rightHalfHeight;  // xyz = right axis (unit),  w = half height
    Vector4 up;               // xyz = up axis (unit)
    Vector4 tint;
    Vector4 frameRectA;       // u0,v0,u1,v1 — current sheet tile
    Vector4 frameRectB;       // u0,v0,u1,v1 — next sheet tile
    Vector4 blendFactor;      // x = blend between tile A and B
};

// CPU-side instance: GPU data plus the texture to bind for this draw.
struct BillboardInstance {
    BillboardInstanceCB cb;
    std::string texturePath; // empty = fallback (white) texture
    bool additive = false;   // false = alpha blend, true = additive (lecture: particle blend modes)
};

// Forward billboard rendering pass.
// Runs alongside the transparent forward mesh pass: alpha-blended,
// back-to-front sorted, sharing the scene colour RT and read-only GBuffer depth.
class BillboardPass {
public:
    static constexpr UINT MAX_BILLBOARDS = 512;

    bool init(ID3D12Device* device);

    // outputRtv must already be bound with the GBuffer read-only depth.
    void render(ID3D12GraphicsCommandList* cmd,
                const std::vector<BillboardInstance>& billboards,
                uint32_t width, uint32_t height);

private:
    bool createUploadBuffer(ID3D12Device* device);
    bool createFallbackTexture(ID3D12Device* device);
    D3D12_GPU_DESCRIPTOR_HANDLE getOrLoadTexture(const std::string& path);

    BillboardPipeline m_pipeline;

    // Per-instance CB ring (upload heap, MAX_BILLBOARDS slots)
    ComPtr<ID3D12Resource> m_cbRing;
    void* m_cbMapped = nullptr;

    // Fallback 1x1 white texture used when a billboard has no texture assigned
    ComPtr<ID3D12Resource> m_fallbackTex;
    ShaderTableDesc m_fallbackSRV;

    // Loaded sprite-sheet textures, cached by source path
    struct CachedTexture {
        ComPtr<ID3D12Resource> resource;
        ShaderTableDesc srv;
    };
    std::unordered_map<std::string, CachedTexture> m_textureCache;
};
