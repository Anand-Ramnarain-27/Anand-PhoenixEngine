#pragma once
#include "TrailPipeline.h"
#include "ComponentTrail.h"
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <unordered_map>
using Microsoft::WRL::ComPtr;

// Per-trail constant buffer — just the matrices/tint the VS needs; vertex
// data (position/uv/colour) comes from the CPU-generated mesh itself.
struct TrailInstanceCB {
    Matrix viewProj;
    Vector4 tint;
};

// CPU-side instance: the generated ribbon mesh plus its draw state.
struct TrailInstance {
    std::vector<ComponentTrail::TrailVertex> vertices;
    Vector4 tint = Vector4(1.f, 1.f, 1.f, 1.f);
    std::string texturePath; // empty = fallback (white) texture
    bool additive = false;
    Vector3 sortPos; // representative world position, for back-to-front sort
    int layer = 0;
};

// Forward trail rendering pass.
// CPU-generated ribbon meshes are uploaded via an upload-heap vertex ring buffer
// each frame and drawn with alpha or additive blending, sharing the scene colour RT
// and read-only GBuffer depth — same setup as the billboard pass.
class TrailPass {
public:
    static constexpr UINT MAX_TRAIL_VERTICES = 1u << 15; // 32768 verts/frame across all trails
    static constexpr UINT MAX_TRAILS = 64;

    bool init(ID3D12Device* device);

    void render(ID3D12GraphicsCommandList* cmd,
                const std::vector<TrailInstance>& trails,
                const Matrix& viewProj,
                uint32_t width, uint32_t height);

private:
    bool createBuffers(ID3D12Device* device);
    bool createFallbackTexture(ID3D12Device* device);
    D3D12_GPU_DESCRIPTOR_HANDLE getOrLoadTexture(const std::string& path);

    TrailPipeline m_pipeline;

    static constexpr UINT kCbAlign = 256u;

    // Vertex ring buffer (upload heap, MAX_TRAIL_VERTICES slots) — persistently mapped.
    ComPtr<ID3D12Resource> m_vbRing;
    void* m_vbMapped = nullptr;
    UINT m_vbStride = 0;

    // Per-draw CB ring (small, reuses the billboard-style pattern).
    ComPtr<ID3D12Resource> m_cbRing;
    void* m_cbMapped = nullptr;

    ComPtr<ID3D12Resource> m_fallbackTex;
    ShaderTableDesc m_fallbackSRV;

    struct CachedTexture {
        ComPtr<ID3D12Resource> resource;
        ShaderTableDesc srv;
    };
    std::unordered_map<std::string, CachedTexture> m_textureCache;
};
