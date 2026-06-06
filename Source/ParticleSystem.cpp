#include "Globals.h"
#include "ParticleSystem.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleGPUResources.h"
#include <d3dx12.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <random>

namespace {
    constexpr UINT cbAlign(UINT b) { return (b + 255u) & ~255u; }

    ComPtr<ID3D12Resource> makeUploadBuf(ID3D12Device* dev, UINT64 sz,
                                          void** mapped, const wchar_t* name) {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(sz);
        ComPtr<ID3D12Resource> b;
        HRESULT hr = dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ,
                                                   nullptr, IID_PPV_ARGS(&b));
        if (FAILED(hr)) return nullptr;
        b->SetName(name);
        if (mapped) b->Map(0, nullptr, mapped);
        return b;
    }

    ComPtr<ID3D12Resource> makeDefaultBuf(ID3D12Device* dev, UINT64 sz,
                                           D3D12_RESOURCE_FLAGS flags,
                                           D3D12_RESOURCE_STATES initialState,
                                           const wchar_t* name) {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(sz, flags);
        ComPtr<ID3D12Resource> b;
        HRESULT hr = dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                   initialState, nullptr, IID_PPV_ARGS(&b));
        if (FAILED(hr)) return nullptr;
        b->SetName(name);
        return b;
    }

    // Simple seeded RNG for CPU-side particle spawn
    thread_local std::mt19937 g_rng{ std::random_device{}() };
    float randF(float lo, float hi) {
        return std::uniform_real_distribution<float>(lo, hi)(g_rng);
    }
}

bool ParticleSystem::init(ID3D12Device* device, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat) {
    if (!m_pipeline.init(device, rtFormat, dsFormat)) {
        LOG("ParticleSystem: pipeline init failed");
        return false;
    }

    // ── GPU particle buffer (DEFAULT heap, RWStructuredBuffer) ──────────────
    const UINT64 particleBytes = MAX_PARTICLES * sizeof(GPUParticle);
    m_particleBuf = makeDefaultBuf(device, particleBytes,
                                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                    L"Particle_Buffer");
    if (!m_particleBuf) { LOG("ParticleSystem: particle buf alloc failed"); return false; }

    // ── Dead-count buffer: 1 × uint (atomic counter) ────────────────────────
    m_deadCountBuf = makeDefaultBuf(device, sizeof(uint32_t) * 4,  // padded
                                     D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                                     D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                     L"Particle_DeadCount");
    if (!m_deadCountBuf) { LOG("ParticleSystem: dead count buf alloc failed"); return false; }

    // ── Upload ring for CPU→GPU particle spawning ───────────────────────────
    m_spawnUpload = makeUploadBuf(device, particleBytes,
                                   reinterpret_cast<void**>(&m_spawnMapped),
                                   L"Particle_SpawnUpload");
    if (!m_spawnUpload) return false;

    // ── CBs ─────────────────────────────────────────────────────────────────
    m_updateCB = makeUploadBuf(device, cbAlign(sizeof(CbUpdate)), &m_updateCBMapped, L"Particle_UpdateCB");
    m_renderCB = makeUploadBuf(device, cbAlign(sizeof(CbRender)), &m_renderCBMapped, L"Particle_RenderCB");
    if (!m_updateCB || !m_renderCB) return false;

    // ── Descriptors ─────────────────────────────────────────────────────────
    auto* sd = app->getShaderDescriptors();
    m_particleUAV = sd->allocTable("Particle_UAV");
    m_particleSRV = sd->allocTable("Particle_SRV");
    m_deadUAV     = sd->allocTable("Particle_DeadUAV");

    // UAV for RWStructuredBuffer<Particle>
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC u = {};
        u.ViewDimension              = D3D12_UAV_DIMENSION_BUFFER;
        u.Format                     = DXGI_FORMAT_UNKNOWN;
        u.Buffer.NumElements         = MAX_PARTICLES;
        u.Buffer.StructureByteStride = sizeof(GPUParticle);
        m_particleUAV.createUAV(m_particleBuf.Get(), 0, &u);
    }
    // SRV for StructuredBuffer<Particle> (read by vertex shader)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC s = {};
        s.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
        s.Format                     = DXGI_FORMAT_UNKNOWN;
        s.Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Buffer.NumElements         = MAX_PARTICLES;
        s.Buffer.StructureByteStride = sizeof(GPUParticle);
        m_particleSRV.createSRV(m_particleBuf.Get(), 0, &s);
    }
    // UAV for RWBuffer<uint> dead count
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC u = {};
        u.ViewDimension      = D3D12_UAV_DIMENSION_BUFFER;
        u.Format             = DXGI_FORMAT_R32_UINT;
        u.Buffer.NumElements = 4;
        m_deadUAV.createUAV(m_deadCountBuf.Get(), 0, &u);
    }

    // ── Fallback 1×1 white sprite ────────────────────────────────────────────
    {
        D3D12_RESOURCE_DESC td = {};
        td.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        td.Width = td.Height= 1;
        td.DepthOrArraySize = 1;
        td.MipLevels        = 1;
        td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc       = { 1, 0 };
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
                                         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                         nullptr, IID_PPV_ARGS(&m_fallbackSprite));
        m_fallbackSpriteSRV = sd->allocTable("Particle_FallbackSprite");
        D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
        sv.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
        sv.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels     = 1;
        m_fallbackSpriteSRV.createSRV(m_fallbackSprite.Get(), 0, &sv);
    }

    m_pendingSpawns.reserve(2048);
    LOG("ParticleSystem: init OK (%u max particles)", MAX_PARTICLES);
    return true;
}

// CPU-side: generate new particle data for this frame's emitters.
void ParticleSystem::spawnParticles(const std::vector<EmitterDesc>& emitters, float dt) {
    m_pendingSpawns.clear();
    for (const auto& e : emitters) {
        int count = int(e.spawnPerSec * dt) + (randF(0.f, 1.f) < fmodf(e.spawnPerSec * dt, 1.f) ? 1 : 0);
        for (int i = 0; i < count; ++i) {
            GPUParticle p;
            p.position    = e.position;
            p.velocity    = Vector3(
                e.velocity.x + randF(-e.velocityVar.x, e.velocityVar.x),
                e.velocity.y + randF(-e.velocityVar.y, e.velocityVar.y),
                e.velocity.z + randF(-e.velocityVar.z, e.velocityVar.z));
            float lt      = e.lifetime + randF(-e.lifetimeVar * 0.5f, e.lifetimeVar * 0.5f);
            p.lifetime    = lt;
            p.maxLifetime = lt;
            p.colour      = e.colourStart;
            m_pendingSpawns.push_back(p);
        }
    }
}

// Copy pending spawns to the GPU particle buffer via CopyBufferRegion.
void ParticleSystem::flushSpawnUploads(ID3D12GraphicsCommandList* cmd) {
    if (m_pendingSpawns.empty()) return;

    UINT count    = (UINT)m_pendingSpawns.size();
    const UINT64 stride = sizeof(GPUParticle);

    for (UINT i = 0; i < count; ++i) {
        UINT slot = m_writeOffset % MAX_PARTICLES;
        m_spawnMapped[slot] = m_pendingSpawns[i];
        m_writeOffset++;
    }

    // Transition particle buf UAV → COPY_DEST
    {
        auto bar = CD3DX12_RESOURCE_BARRIER::Transition(m_particleBuf.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
        cmd->ResourceBarrier(1, &bar);
    }

    // Copy from upload ring to GPU particle buffer (wrap-around handled by writing to slot)
    UINT written = 0;
    UINT startSlot = (m_writeOffset - count) % MAX_PARTICLES;
    while (written < count) {
        UINT chunk = std::min(count - written, MAX_PARTICLES - startSlot);
        cmd->CopyBufferRegion(m_particleBuf.Get(), startSlot * stride,
                               m_spawnUpload.Get(), startSlot * stride,
                               chunk * stride);
        written   += chunk;
        startSlot  = (startSlot + chunk) % MAX_PARTICLES;
    }

    // Restore particle buf to UAV
    {
        auto bar = CD3DX12_RESOURCE_BARRIER::Transition(m_particleBuf.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &bar);
    }
}

void ParticleSystem::update(ID3D12GraphicsCommandList* cmd,
                             float deltaTime,
                             const std::vector<EmitterDesc>& emitters) {
    spawnParticles(emitters, deltaTime);
    flushSpawnUploads(cmd);

    // Upload CbUpdate
    {
        // Use the first emitter's gravity/colours, or defaults if none
        CbUpdate cb = {};
        if (!emitters.empty()) {
            const auto& e = emitters[0];
            cb.gravity      = e.gravity;
            cb.colourStart  = e.colourStart;
            cb.colourEnd    = e.colourEnd;
        } else {
            cb.gravity      = Vector3(0.f, -4.f, 0.f);
            cb.colourStart  = Vector4(1.f, 0.5f, 0.1f, 1.f);
            cb.colourEnd    = Vector4(0.1f, 0.1f, 0.5f, 0.f);
        }
        cb.deltaTime    = deltaTime;
        cb.maxParticles = MAX_PARTICLES;
        memcpy(m_updateCBMapped, &cb, sizeof(cb));
    }

    BEGIN_EVENT(cmd, L"Particle Update");

    // Bind compute pipeline
    cmd->SetPipelineState(m_pipeline.getUpdatePSO());
    cmd->SetComputeRootSignature(m_pipeline.getComputeRootSig());

    ID3D12DescriptorHeap* heap[] = { app->getShaderDescriptors()->getHeap() };
    cmd->SetDescriptorHeaps(1, heap);

    cmd->SetComputeRootConstantBufferView(ParticlePipeline::CS_SLOT_CB,
                                           m_updateCB->GetGPUVirtualAddress());
    cmd->SetComputeRootDescriptorTable(ParticlePipeline::CS_SLOT_PARTICLES,
                                        m_particleUAV.getGPUHandle(0));
    cmd->SetComputeRootDescriptorTable(ParticlePipeline::CS_SLOT_DEAD_COUNT,
                                        m_deadUAV.getGPUHandle(0));

    // Lecture group-count formula: (N + threads - 1) / threads
    UINT groups = (MAX_PARTICLES + UPDATE_THREADS - 1) / UPDATE_THREADS;
    cmd->Dispatch(groups, 1, 1);

    END_EVENT(cmd);

    // UAV barrier: ensure all particle writes are visible before the render VS reads them
    {
        auto bar = CD3DX12_RESOURCE_BARRIER::UAV(m_particleBuf.Get());
        cmd->ResourceBarrier(1, &bar);
    }
}

void ParticleSystem::render(ID3D12GraphicsCommandList* cmd,
                             const Matrix& viewProj,
                             const Vector3& cameraRight,
                             const Vector3& cameraUp,
                             float halfSize) {
    // Upload CbRender
    {
        CbRender cb = {};
        cb.viewProj     = viewProj.Transpose();
        cb.cameraRight  = cameraRight;
        cb.halfSize     = halfSize;
        cb.cameraUp     = cameraUp;
        memcpy(m_renderCBMapped, &cb, sizeof(cb));
    }

    BEGIN_EVENT(cmd, L"Particle Render");

    // Transition particle buf: UAV → SRV (vertex shader reads it as StructuredBuffer)
    {
        auto bar = CD3DX12_RESOURCE_BARRIER::Transition(m_particleBuf.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &bar);
    }

    ID3D12DescriptorHeap* heaps[] = {
        app->getShaderDescriptors()->getHeap(),
        app->getSamplerHeap()->getHeap()
    };
    cmd->SetDescriptorHeaps(2, heaps);

    cmd->SetPipelineState(m_pipeline.getRenderPSO());
    cmd->SetGraphicsRootSignature(m_pipeline.getGraphicsRootSig());

    cmd->SetGraphicsRootConstantBufferView(ParticlePipeline::GFX_SLOT_CB,
                                            m_renderCB->GetGPUVirtualAddress());
    cmd->SetGraphicsRootDescriptorTable(ParticlePipeline::GFX_SLOT_PARTICLES,
                                         m_particleSRV.getGPUHandle(0));
    cmd->SetGraphicsRootDescriptorTable(ParticlePipeline::GFX_SLOT_SPRITE,
                                         m_fallbackSpriteSRV.getGPUHandle(0));
    cmd->SetGraphicsRootDescriptorTable(ParticlePipeline::GFX_SLOT_SAMPLER,
        app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    // 6 vertices per particle (two triangles), MAX_PARTICLES instances.
    // Dead particles collapse to degenerate triangles in the VS.
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->DrawInstanced(6, MAX_PARTICLES, 0, 0);

    END_EVENT(cmd);

    // Restore particle buf to UAV for next frame's update CS
    {
        auto bar = CD3DX12_RESOURCE_BARRIER::Transition(m_particleBuf.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &bar);
    }
}
