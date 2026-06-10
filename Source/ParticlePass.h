#pragma once
#include "ParticlePipeline.h"
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <unordered_map>
using Microsoft::WRL::ComPtr;

// ---- GpuParticle -------------------------------------------------------
// Per-particle data uploaded to the GPU each frame.
// Mirrors the HLSL struct in ParticleRenderVS.hlsl / ParticleUpdateCS.hlsl.
// All over-lifetime curves (color, size) are evaluated on the CPU before upload
// so the GPU shaders stay simple and the data is self-contained.
//
// Layout (52 bytes):
//   float3 position   12
//   float  size        4   → 16
//   float4 color      16   → 32
//   float  rotation    4
//   float2 uvMin       8
//   float2 uvMax       8   → 52
struct GpuParticle {
    float position[3];   // world-space centre
    float size;          // world-space diameter
    float color[4];      // RGBA (baked over-lifetime color * alpha)
    float rotation;      // degrees (applied in VS around view axis)
    float uvMin[2];      // sheet atlas tile — bottom-left UV
    float uvMax[2];      // sheet atlas tile — top-right UV
};
static_assert(sizeof(GpuParticle) == 52, "GpuParticle size mismatch");

// ---- CbParticle --------------------------------------------------------
// Per-emitter constant buffer uploaded once per draw.
struct CbParticle {
    Matrix viewProj;
    Vector4 camRight;
    Vector4 camUp;
};

// ---- CbParticleUpdate --------------------------------------------------
// Constant buffer for the turbulence compute dispatch.
struct CbParticleUpdate {
    UINT  activeCount;
    float deltaTime;
    float turbFrequency;
    float turbStrength;
    float turbScrollSpeed;
    float time;
    float pad[2];
};
static_assert(sizeof(CbParticleUpdate) % 16 == 0, "CbParticleUpdate must be 16-byte aligned");

// ---- ParticleDrawRequest ------------------------------------------------
// One entry per ComponentParticleSystem that has useGPU = true.
struct ParticleDrawRequest {
    std::vector<GpuParticle> particles;     // baked particle data for this frame
    std::string texturePath;                // empty = fallback white
    bool additive = false;
    bool gpuTurbulence = false;             // dispatch ParticleUpdateCS before rendering
    float turbFrequency   = 0.5f;
    float turbStrength    = 1.5f;
    float turbScrollSpeed = 0.3f;
    float deltaTime       = 0.f;
    float time            = 0.f;
    // Stable key for buffer caching — use emitter pointer cast to size_t.
    size_t emitterKey = 0;
    int    maxParticles = 256;              // buffer capacity hint
};

// ---- ParticlePass -------------------------------------------------------
// GPU particle rendering pass.
//
// For each ParticleDrawRequest (one per emitter, collected by ModuleEditor):
//   1. Uploads the CPU-baked GpuParticle array to a persistently-mapped upload
//      heap StructuredBuffer.
//   2. If gpuTurbulence: dispatches ParticleUpdateCS (noise flow field) which
//      reads the upload SRV and writes to a UAV default-heap buffer.  A UAV
//      barrier follows the dispatch.  The render reads from the UAV buffer.
//   3. If !gpuTurbulence: the render reads directly from the upload buffer.
//   4. Issues DrawInstanced(4, liveCount, 0, 0) with TRIANGLESTRIP — one quad
//      per particle, no index buffer.
//
// All N particles of an emitter are rendered in one draw call (vs. N draw calls
// in the BillboardPass-per-particle approach).
class ParticlePass {
public:
    static constexpr UINT MAX_PARTICLES_PER_EMITTER = 4096;
    static constexpr UINT MAX_EMITTERS              = 32;

    bool init(ID3D12Device* device);

    void render(ID3D12GraphicsCommandList* cmd,
                const std::vector<ParticleDrawRequest>& requests,
                const Matrix& viewProj,
                const Vector3& camRight, const Vector3& camUp,
                float globalTime,
                uint32_t width, uint32_t height);

private:
    // ---- Per-emitter GPU buffers ----------------------------------------
    struct EmitterBuffers {
        // Upload heap — CPU writes to this every frame, render VS reads it
        // when no CS is used.
        ComPtr<ID3D12Resource> uploadBuf;
        void*                  uploadMapped = nullptr;
        ShaderTableDesc        uploadSRV;   // t0 for render (no-CS path)

        // Default heap UAV — CS writes here, render VS reads from here
        // when gpuTurbulence is true.
        ComPtr<ID3D12Resource> uavBuf;
        ShaderTableDesc        uavSRV;      // t0 for render (CS path)
        ShaderTableDesc        uavUAV;      // u0 for CS output

        UINT capacity = 0;  // allocated GpuParticle slots
    };

    EmitterBuffers& getOrCreateBuffers(ID3D12Device* device, size_t key, UINT capacity);

    // ---- Per-frame constant buffer ring ---------------------------------
    bool createCbRing(ID3D12Device* device);
    bool createFallbackTexture(ID3D12Device* device);
    D3D12_GPU_DESCRIPTOR_HANDLE getOrLoadTexture(const std::string& path);

    ParticlePipeline m_pipeline;

    static constexpr UINT kCbAlign = 256u;

    // CB ring for per-draw (gfx) and per-dispatch (cs) constants.
    ComPtr<ID3D12Resource> m_cbRing;
    void*                  m_cbMapped = nullptr;

    // Fallback 1×1 white texture.
    ComPtr<ID3D12Resource> m_fallbackTex;
    ShaderTableDesc        m_fallbackSRV;

    struct CachedTexture {
        ComPtr<ID3D12Resource> resource;
        ShaderTableDesc        srv;
    };
    std::unordered_map<std::string, CachedTexture> m_textureCache;
    std::unordered_map<size_t, EmitterBuffers>     m_emitterBuffers;
};
