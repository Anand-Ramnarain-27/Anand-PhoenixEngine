#include "Globals.h"
#include "ParticlePass.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleGPUResources.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleEditor.h"
#include <imgui.h>
#include <d3dx12.h>
#include <algorithm>
#include <cstring>

namespace {
    constexpr UINT cbAlign(UINT b){ return (b + 255u) & ~255u; }
}

bool ParticlePass::init(ID3D12Device* device){
    if (!m_pipeline.init(device)){
        LOG("ParticlePass: pipeline init failed");
        return false;
    }
    if (!createCbRing(device)) return false;
    if (!createFallbackTexture(device)) return false;

    LOG("ParticlePass: init OK");
    if (auto* ed = app->getEditor())
        ed->log("ParticlePass: initialized OK", ImVec4(0.5f, 1.f, 0.5f, 1.f));
    return true;
}

bool ParticlePass::createCbRing(ID3D12Device* device){
    const UINT slots = MAX_EMITTERS * 2;
    const UINT64 total = (UINT64)cbAlign(sizeof(CbParticle)) * slots;
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bd = CD3DX12_RESOURCE_DESC::Buffer(total);
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
                                                  nullptr, IID_PPV_ARGS(&m_cbRing));
    if (FAILED(hr)){ LOG("ParticlePass: CB ring alloc failed 0x%08X", hr); return false; }
    m_cbRing->SetName(L"Particle_CBRing");
    m_cbRing->Map(0, nullptr, &m_cbMapped);
    return true;
}

bool ParticlePass::createFallbackTexture(ID3D12Device* device){
    (void)device;
    const uint32_t white = 0xFFFFFFFFu;
    m_fallbackTex = app->getGPUResources()->createRawTexture2D(&white, sizeof(white), 1, 1,
                                                                DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!m_fallbackTex){ LOG("ParticlePass: fallback texture creation failed"); return false; }
    m_fallbackTex->SetName(L"Particle_FallbackTex");

    auto* sd = app->getShaderDescriptors();
    m_fallbackSRV = sd->allocTable("Particle_FallbackSRV");
    if (!m_fallbackSRV.isValid()){ LOG("ParticlePass: fallback SRV alloc failed"); return false; }

    D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
    sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2D.MipLevels = 1;
    m_fallbackSRV.createSRV(m_fallbackTex.Get(), 0, &sv);
    return true;
}

D3D12_GPU_DESCRIPTOR_HANDLE ParticlePass::getOrLoadTexture(const std::string& path){
    if (path.empty()) return m_fallbackSRV.getGPUHandle(0);

    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end()) return it->second.srv.getGPUHandle(0);

    auto* gpu = app->getGPUResources();
    ComPtr<ID3D12Resource> tex = gpu ? gpu->createTextureFromFile(path, true) : nullptr;
    if (!tex){
        LOG("ParticlePass: failed to load texture '%s', using fallback", path.c_str());
        m_textureCache.emplace(path, CachedTexture{ nullptr, m_fallbackSRV });
        return m_fallbackSRV.getGPUHandle(0);
    }

    ShaderTableDesc srv = app->getShaderDescriptors()->allocTable(("Particle_SRV_" + path).c_str());
    if (!srv.isValid()){
        LOG("ParticlePass: SRV alloc failed for '%s', using fallback", path.c_str());
        m_textureCache.emplace(path, CachedTexture{ nullptr, m_fallbackSRV });
        return m_fallbackSRV.getGPUHandle(0);
    }
    srv.createTexture2DSRV(tex.Get(), 0);

    auto handle = srv.getGPUHandle(0);
    m_textureCache.emplace(path, CachedTexture{ std::move(tex), std::move(srv) });
    return handle;
}

ParticlePass::EmitterBuffers& ParticlePass::getOrCreateBuffers(ID3D12Device* device,
                                                                size_t key, UINT capacity){
    capacity = std::min(capacity, MAX_PARTICLES_PER_EMITTER);

    auto it = m_emitterBuffers.find(key);
    if (it != m_emitterBuffers.end() && it->second.capacity >= capacity)
        return it->second;

    if (it != m_emitterBuffers.end())
        m_emitterBuffers.erase(it);

    EmitterBuffers& eb = m_emitterBuffers[key];
    eb.capacity = capacity;

    const UINT64 bufSize = (UINT64)sizeof(GpuParticle) * capacity;

    {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(bufSize);
        if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&eb.uploadBuf)))){
            LOG("ParticlePass: upload buf alloc failed for key %zu", key);
            return eb;
        }
        eb.uploadBuf->SetName(L"Particle_Upload");
        eb.uploadBuf->Map(0, nullptr, &eb.uploadMapped);

        eb.uploadSRV = app->getShaderDescriptors()->allocTable("Particle_UploadSRV");
        if (eb.uploadSRV.isValid()){
            D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
            sv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            sv.Format = DXGI_FORMAT_UNKNOWN;
            sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            sv.Buffer.NumElements = capacity;
            sv.Buffer.StructureByteStride = sizeof(GpuParticle);
            eb.uploadSRV.createSRV(eb.uploadBuf.Get(), 0, &sv);
        }
    }

    {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(bufSize,
                      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&eb.uavBuf)))){
            LOG("ParticlePass: UAV buf alloc failed for key %zu", key);
            return eb;
        }
        eb.uavBuf->SetName(L"Particle_UAV");

        auto* sd = app->getShaderDescriptors();

        eb.uavSRV = sd->allocTable("Particle_UAVSRV");
        if (eb.uavSRV.isValid()){
            D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
            sv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            sv.Format = DXGI_FORMAT_UNKNOWN;
            sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            sv.Buffer.NumElements = capacity;
            sv.Buffer.StructureByteStride = sizeof(GpuParticle);
            eb.uavSRV.createSRV(eb.uavBuf.Get(), 0, &sv);
        }

        eb.uavUAV = sd->allocTable("Particle_UAV");
        if (eb.uavUAV.isValid()){
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
            uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uav.Format = DXGI_FORMAT_UNKNOWN;
            uav.Buffer.NumElements = capacity;
            uav.Buffer.StructureByteStride = sizeof(GpuParticle);
            eb.uavUAV.createUAV(eb.uavBuf.Get(), 0, &uav);
        }
    }

    return eb;
}

void ParticlePass::render(ID3D12GraphicsCommandList* cmd,
                           const std::vector<ParticleDrawRequest>& requests,
                           const Matrix& viewProj,
                           const Vector3& camRight, const Vector3& camUp,
                           float globalTime,
                           uint32_t width, uint32_t height){
    if (requests.empty() || width == 0 || height == 0) return;

    BEGIN_EVENT(cmd, L"Particle GPU Pass");

    auto* device = app->getD3D12()->getDevice();
    auto* sd = app->getShaderDescriptors();
    auto* sh = app->getSamplerHeap();

    ID3D12DescriptorHeap* heaps[] = { sd->getHeap(), sh->getHeap() };

    const UINT cbStride = cbAlign(sizeof(CbParticle));
    UINT cbSlot = 0;
    UINT drawCount = 0;


    for (const auto& req : requests){
        if (req.particles.empty()) continue;
        if (drawCount >= MAX_EMITTERS) break;

        const UINT liveCount = std::min((UINT)req.particles.size(), MAX_PARTICLES_PER_EMITTER);
        EmitterBuffers& eb = getOrCreateBuffers(device, req.emitterKey,
                                                std::max(liveCount, (UINT)req.maxParticles));
        if (!eb.uploadBuf || !eb.uploadMapped) continue;

        memcpy(eb.uploadMapped, req.particles.data(), sizeof(GpuParticle) * liveCount);

        D3D12_GPU_DESCRIPTOR_HANDLE renderParticlesSRV;
        if (req.gpuTurbulence && eb.uavBuf && eb.uavUAV.isValid() && eb.uploadSRV.isValid()){
            cmd->SetComputeRootSignature(m_pipeline.getCsRootSig());
            cmd->SetDescriptorHeaps(2, heaps);
            cmd->SetPipelineState(m_pipeline.getComputePSO());

            CbParticleUpdate cbUpdate = {};
            cbUpdate.activeCount = liveCount;
            cbUpdate.deltaTime = req.deltaTime;
            cbUpdate.turbFrequency = req.turbFrequency;
            cbUpdate.turbStrength = req.turbStrength;
            cbUpdate.turbScrollSpeed = req.turbScrollSpeed;
            cbUpdate.time = req.time;
            void* cbDst = reinterpret_cast<uint8_t*>(m_cbMapped) + cbSlot * cbStride;
            memcpy(cbDst, &cbUpdate, sizeof(CbParticleUpdate));
            D3D12_GPU_VIRTUAL_ADDRESS cbVA = m_cbRing->GetGPUVirtualAddress() + cbSlot * cbStride;
            cmd->SetComputeRootConstantBufferView(ParticlePipeline::CS_SLOT_CB, cbVA);
            cmd->SetComputeRootDescriptorTable(ParticlePipeline::CS_SLOT_INPUT,
                                               eb.uploadSRV.getGPUHandle(0));
            cmd->SetComputeRootDescriptorTable(ParticlePipeline::CS_SLOT_OUTPUT,
                                               eb.uavUAV.getGPUHandle(0));
            ++cbSlot;

            const UINT groups = (liveCount + 63) / 64;
            cmd->Dispatch(groups, 1, 1);

            D3D12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(eb.uavBuf.Get());
            cmd->ResourceBarrier(1, &uavBarrier);

            D3D12_RESOURCE_BARRIER toSRV = CD3DX12_RESOURCE_BARRIER::Transition(
                eb.uavBuf.Get(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cmd->ResourceBarrier(1, &toSRV);

            renderParticlesSRV = eb.uavSRV.getGPUHandle(0);
        } else {
            renderParticlesSRV = eb.uploadSRV.isValid()
                               ? eb.uploadSRV.getGPUHandle(0)
                               : m_fallbackSRV.getGPUHandle(0);
        }

        cmd->SetGraphicsRootSignature(m_pipeline.getGfxRootSig());
        cmd->SetDescriptorHeaps(2, heaps);
        cmd->SetPipelineState(req.additive ? m_pipeline.getAdditivePSO() : m_pipeline.getPSO());

        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        cmd->IASetVertexBuffers(0, 0, nullptr);
        cmd->IASetIndexBuffer(nullptr);

        CbParticle cb = {};
        cb.viewProj = viewProj;
        cb.camRight = Vector4(camRight.x, camRight.y, camRight.z, 0.f);
        cb.camUp = Vector4(camUp.x, camUp.y, camUp.z, 0.f);
        void* cbDst = reinterpret_cast<uint8_t*>(m_cbMapped) + cbSlot * cbStride;
        memcpy(cbDst, &cb, sizeof(CbParticle));
        D3D12_GPU_VIRTUAL_ADDRESS cbVA = m_cbRing->GetGPUVirtualAddress() + cbSlot * cbStride;
        cmd->SetGraphicsRootConstantBufferView(ParticlePipeline::GFX_SLOT_CB, cbVA);
        cmd->SetGraphicsRootDescriptorTable(ParticlePipeline::GFX_SLOT_PARTICLES, renderParticlesSRV);
        cmd->SetGraphicsRootDescriptorTable(ParticlePipeline::GFX_SLOT_TEXTURE,
                                             getOrLoadTexture(req.texturePath));
        cmd->SetGraphicsRootDescriptorTable(ParticlePipeline::GFX_SLOT_SAMPLER,
            sh->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

        cmd->DrawInstanced(4, liveCount, 0, 0);
        ++cbSlot;
        ++drawCount;

        if (req.gpuTurbulence && eb.uavBuf){
            D3D12_RESOURCE_BARRIER toUAV = CD3DX12_RESOURCE_BARRIER::Transition(
                eb.uavBuf.Get(),
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            cmd->ResourceBarrier(1, &toUAV);
        }
    }

    END_EVENT(cmd);
}
