#pragma once
#include <d3d12.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

class ParticlePipeline {
public:
    static constexpr UINT GFX_SLOT_CB = 0;
    static constexpr UINT GFX_SLOT_PARTICLES = 1;
    static constexpr UINT GFX_SLOT_TEXTURE = 2;
    static constexpr UINT GFX_SLOT_SAMPLER = 3;

    static constexpr UINT CS_SLOT_CB = 0;
    static constexpr UINT CS_SLOT_INPUT = 1;
    static constexpr UINT CS_SLOT_OUTPUT = 2;

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12PipelineState* getAdditivePSO() const { return m_additivePso.Get(); }
    ID3D12PipelineState* getComputePSO() const { return m_computePso.Get(); }
    ID3D12RootSignature* getGfxRootSig() const { return m_gfxRootSig.Get(); }
    ID3D12RootSignature* getCsRootSig() const { return m_csRootSig.Get(); }

private:
    bool createGfxRootSignature(ID3D12Device* device);
    bool createCsRootSignature(ID3D12Device* device);
    bool createGraphicsPSOs(ID3D12Device* device);
    bool createComputePSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_gfxRootSig;
    ComPtr<ID3D12RootSignature> m_csRootSig;
    ComPtr<ID3D12PipelineState> m_pso;
    ComPtr<ID3D12PipelineState> m_additivePso;
    ComPtr<ID3D12PipelineState> m_computePso;
};
#include "ShaderTableDesc.h"
#include "Globals.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <unordered_map>
using Microsoft::WRL::ComPtr;

struct GpuParticle {
    float position[3];
    float size;
    float color[4];
    float rotation;
    float uvMin[2];
    float uvMax[2];
};
static_assert(sizeof(GpuParticle) == 52, "GpuParticle size mismatch");

struct CbParticle {
    Matrix viewProj;
    Vector4 camRight;
    Vector4 camUp;
};

struct CbParticleUpdate {
    UINT activeCount;
    float deltaTime;
    float turbFrequency;
    float turbStrength;
    float turbScrollSpeed;
    float time;
    float pad[2];
};
static_assert(sizeof(CbParticleUpdate) % 16 == 0, "CbParticleUpdate must be 16-byte aligned");

struct ParticleDrawRequest {
    std::vector<GpuParticle> particles;
    std::string texturePath;
    bool additive = false;
    bool gpuTurbulence = false;
    float turbFrequency = 0.5f;
    float turbStrength = 1.5f;
    float turbScrollSpeed = 0.3f;
    float deltaTime = 0.f;
    float time = 0.f;
    size_t emitterKey = 0;
    int maxParticles = 256;
};

class ParticlePass {
public:
    static constexpr UINT MAX_PARTICLES_PER_EMITTER = 4096;
    static constexpr UINT MAX_EMITTERS = 32;

    bool init(ID3D12Device* device);

    // Call once per frame before any render() calls to reset the ring-buffer cursor.
    void beginFrame() { m_frameCBCursor = 0; }

    void render(ID3D12GraphicsCommandList* cmd,
                const std::vector<ParticleDrawRequest>& requests,
                const Matrix& viewProj,
                const Vector3& camRight, const Vector3& camUp,
                float globalTime,
                uint32_t width, uint32_t height);

private:
    struct EmitterBuffers {
        ComPtr<ID3D12Resource> uploadBuf;
        void* uploadMapped = nullptr;
        ShaderTableDesc uploadSRV;

        ComPtr<ID3D12Resource> uavBuf;
        ShaderTableDesc uavSRV;
        ShaderTableDesc uavUAV;

        UINT capacity = 0;
    };

    EmitterBuffers& getOrCreateBuffers(ID3D12Device* device, size_t key, UINT capacity);

    bool createCbRing(ID3D12Device* device);
    bool createFallbackTexture(ID3D12Device* device);
    D3D12_GPU_DESCRIPTOR_HANDLE getOrLoadTexture(const std::string& path);

    ParticlePipeline m_pipeline;

    static constexpr UINT kCbAlign = 256u;

    ComPtr<ID3D12Resource> m_cbRing;
    void* m_cbMapped = nullptr;

    ComPtr<ID3D12Resource> m_fallbackTex;
    ShaderTableDesc m_fallbackSRV;

    struct CachedTexture {
        ComPtr<ID3D12Resource> resource;
        ShaderTableDesc srv;
    };
    std::unordered_map<std::string, CachedTexture> m_textureCache;
    std::unordered_map<size_t, EmitterBuffers> m_emitterBuffers;

    // Frame-persistent cursor — advanced by render(), reset by beginFrame().
    UINT m_frameCBCursor = 0;
};

