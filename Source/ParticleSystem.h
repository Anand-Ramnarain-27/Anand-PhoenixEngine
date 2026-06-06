#pragma once
#include "ParticlePipeline.h"
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
using Microsoft::WRL::ComPtr;

// Describes one emitter's spawn parameters (CPU side).
struct EmitterDesc {
    Vector3 position    = Vector3::Zero;
    Vector3 velocity    = Vector3(0.f, 4.f, 0.f);   // base velocity
    Vector3 velocityVar = Vector3(1.f, 1.f, 1.f);   // ±random spread
    Vector3 gravity     = Vector3(0.f, -4.f, 0.f);
    Vector4 colourStart = Vector4(1.f, 0.6f, 0.1f, 1.f);  // orange
    Vector4 colourEnd   = Vector4(0.2f, 0.1f, 0.8f, 0.f); // blue/transparent
    float   lifetime    = 2.5f;
    float   lifetimeVar = 1.0f;
    float   halfSize    = 0.08f;
    int     spawnPerSec = 120;
};

// GPU Particle System.
// Demonstrates Lecture 14 concepts:
//   - RWStructuredBuffer (UAV) for particle state — readable by VS as SRV after barrier
//   - InterlockedAdd (atomic) for dead-particle counting
//   - [numthreads(256,1,1)] with (N+255)/256 group count
//   - UAV→SRV resource state transition between compute and graphics pipeline
//   - CPU spawning via upload heap ring-buffer
class ParticleSystem {
public:
    static constexpr UINT MAX_PARTICLES = 65536;
    static constexpr UINT UPDATE_THREADS = 256;

    // GPU-side particle layout (must match Particle.hlsli)
    struct GPUParticle {
        Vector3 position;   float lifetime;
        Vector3 velocity;   float maxLifetime;
        Vector4 colour;
    };

    struct CbUpdate {
        Vector3  gravity;      float deltaTime;
        Vector4  colourStart;
        Vector4  colourEnd;
        uint32_t maxParticles; float pad[3];
    };

    struct CbRender {
        Matrix  viewProj;
        Vector3 cameraRight; float halfSize;
        Vector3 cameraUp;    float cbPad;
    };

    bool init(ID3D12Device* device, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat);

    // Spawn new particles for active emitters, then simulate all particles.
    void update(ID3D12GraphicsCommandList* cmd,
                float deltaTime,
                const std::vector<EmitterDesc>& emitters);

    // Draw all active particles as camera-facing billboards.
    // sceneRT must be bound as RTV on entry.
    void render(ID3D12GraphicsCommandList* cmd,
                const Matrix& viewProj,
                const Vector3& cameraRight,
                const Vector3& cameraUp,
                float halfSize = 0.08f);

    uint32_t getAliveCount() const { return m_aliveCount; }

private:
    void spawnParticles(const std::vector<EmitterDesc>& emitters, float dt);
    void flushSpawnUploads(ID3D12GraphicsCommandList* cmd);

    ParticlePipeline m_pipeline;

    // GPU DEFAULT heap buffers
    ComPtr<ID3D12Resource> m_particleBuf;     // RWStructuredBuffer<Particle>
    ComPtr<ID3D12Resource> m_deadCountBuf;    // RWBuffer<uint>  (atomic)

    // Upload ring for spawning new particles each frame
    ComPtr<ID3D12Resource> m_spawnUpload;
    GPUParticle*           m_spawnMapped   = nullptr;
    uint32_t               m_spawnHead     = 0;   // next free slot in upload ring

    // CBs (upload, persistently mapped)
    ComPtr<ID3D12Resource> m_updateCB;
    void*                  m_updateCBMapped = nullptr;
    ComPtr<ID3D12Resource> m_renderCB;
    void*                  m_renderCBMapped = nullptr;

    // Descriptors
    ShaderTableDesc m_particleUAV;   // for compute
    ShaderTableDesc m_particleSRV;   // for render VS
    ShaderTableDesc m_deadUAV;
    ShaderTableDesc m_fallbackSpriteSRV;  // 1×1 white texture

    ComPtr<ID3D12Resource> m_fallbackSprite;

    // CPU-side ring of particles waiting to be copied to GPU
    std::vector<GPUParticle> m_pendingSpawns;
    uint32_t m_aliveCount  = 0;
    uint32_t m_writeOffset = 0;   // next particle slot to overwrite (ring)
};
