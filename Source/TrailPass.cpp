#include "Globals.h"
#include "TrailPass.h"
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
    constexpr UINT cbAlign(UINT b) { return (b + 255u) & ~255u; }
}

bool TrailPass::init(ID3D12Device* device){
    if (!m_pipeline.init(device)) {
        LOG("TrailPass: pipeline init failed");
        return false;
    }
    if (!createBuffers(device)) return false;
    if (!createFallbackTexture(device)) return false;

    LOG("TrailPass: init OK");
    if (auto* ed = app->getEditor())
        ed->log("TrailPass: initialized OK", ImVec4(0.5f, 1.f, 0.5f, 1.f));
    return true;
}

bool TrailPass::createBuffers(ID3D12Device* device){
    m_vbStride = sizeof(ComponentTrail::TrailVertex);
    const UINT64 vbTotal = (UINT64)m_vbStride * MAX_TRAIL_VERTICES;
    {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(vbTotal);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr, IID_PPV_ARGS(&m_vbRing));
        if (FAILED(hr)) { LOG("TrailPass: VB ring alloc failed 0x%08X", hr); return false; }
        m_vbRing->SetName(L"Trail_VBRing");
        m_vbRing->Map(0, nullptr, &m_vbMapped);
    }
    {
        const UINT stride = cbAlign(sizeof(TrailInstanceCB));
        const UINT64 total = (UINT64)stride * MAX_TRAILS;
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(total);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr, IID_PPV_ARGS(&m_cbRing));
        if (FAILED(hr)) { LOG("TrailPass: CB ring alloc failed 0x%08X", hr); return false; }
        m_cbRing->SetName(L"Trail_CBRing");
        m_cbRing->Map(0, nullptr, &m_cbMapped);
    }
    return true;
}

bool TrailPass::createFallbackTexture(ID3D12Device* device){
    (void)device;
    const uint32_t white = 0xFFFFFFFFu;
    m_fallbackTex = app->getGPUResources()->createRawTexture2D(&white, sizeof(white), 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!m_fallbackTex) {
        LOG("TrailPass: fallback texture creation failed");
        return false;
    }
    m_fallbackTex->SetName(L"Trail_FallbackTex");

    auto* sd = app->getShaderDescriptors();
    m_fallbackSRV = sd->allocTable("Trail_FallbackSRV");
    if (!m_fallbackSRV.isValid()) {
        LOG("TrailPass: fallback SRV alloc failed");
        return false;
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
    sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2D.MipLevels = 1;
    m_fallbackSRV.createSRV(m_fallbackTex.Get(), 0, &sv);
    return true;
}

D3D12_GPU_DESCRIPTOR_HANDLE TrailPass::getOrLoadTexture(const std::string& path){
    if (path.empty())
        return m_fallbackSRV.getGPUHandle(0);

    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end())
        return it->second.srv.getGPUHandle(0);

    auto* gpu = app->getGPUResources();
    ComPtr<ID3D12Resource> tex = gpu ? gpu->createTextureFromFile(path, true) : nullptr;
    if (!tex) {
        LOG("TrailPass: failed to load texture '%s', using fallback", path.c_str());
        m_textureCache.emplace(path, CachedTexture{ nullptr, m_fallbackSRV });
        return m_fallbackSRV.getGPUHandle(0);
    }

    ShaderTableDesc srv = app->getShaderDescriptors()->allocTable(("Trail_SRV_" + path).c_str());
    if (!srv.isValid()) {
        LOG("TrailPass: SRV alloc failed for '%s', using fallback", path.c_str());
        m_textureCache.emplace(path, CachedTexture{ nullptr, m_fallbackSRV });
        return m_fallbackSRV.getGPUHandle(0);
    }
    srv.createTexture2DSRV(tex.Get(), 0);

    auto handle = srv.getGPUHandle(0);
    m_textureCache.emplace(path, CachedTexture{ std::move(tex), std::move(srv) });
    return handle;
}

void TrailPass::render(ID3D12GraphicsCommandList* cmd,
                       const std::vector<TrailInstance>& trails,
                       const Matrix& viewProj,
                       uint32_t width, uint32_t height){
    if (trails.empty() || width == 0 || height == 0) return;

    BEGIN_EVENT(cmd, L"Trail Pass");

    cmd->SetGraphicsRootSignature(m_pipeline.getRootSig());

    ID3D12DescriptorHeap* heaps[] = {
        app->getShaderDescriptors()->getHeap(),
        app->getSamplerHeap()->getHeap()
    };
    cmd->SetDescriptorHeaps(2, heaps);

    cmd->SetGraphicsRootDescriptorTable(TrailPipeline::SLOT_SAMPLER,
        app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetIndexBuffer(nullptr);

    const UINT cbStride = cbAlign(sizeof(TrailInstanceCB));
    const UINT maxDraws = std::min((UINT)trails.size(), MAX_TRAILS);

    bool additiveBound = false;
    cmd->SetPipelineState(m_pipeline.getPSO());

    UINT vertexCursor = 0;
    for (UINT i = 0; i < maxDraws; ++i) {
        const TrailInstance& tr = trails[i];
        const UINT vCount = (UINT)tr.vertices.size();
        if (vCount == 0) continue;
        if (vertexCursor + vCount > MAX_TRAIL_VERTICES) break; // ring exhausted this frame

        if (tr.additive != additiveBound) {
            additiveBound = tr.additive;
            cmd->SetPipelineState(additiveBound ? m_pipeline.getAdditivePSO() : m_pipeline.getPSO());
        }

        // Upload vertices for this trail at the current cursor position.
        uint8_t* dstV = reinterpret_cast<uint8_t*>(m_vbMapped) + (size_t)vertexCursor * m_vbStride;
        memcpy(dstV, tr.vertices.data(), (size_t)vCount * m_vbStride);

        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = m_vbRing->GetGPUVirtualAddress() + (UINT64)vertexCursor * m_vbStride;
        vbv.SizeInBytes = vCount * m_vbStride;
        vbv.StrideInBytes = m_vbStride;
        cmd->IASetVertexBuffers(0, 1, &vbv);

        // Per-draw constants — viewProj is identical across instances this frame
        // but kept per-CB (matches the billboard pass layout and leaves room for
        // future per-trail transforms / tints).
        TrailInstanceCB cb{};
        cb.viewProj = viewProj;
        cb.tint = tr.tint;
        void* dstCb = reinterpret_cast<uint8_t*>(m_cbMapped) + i * cbStride;
        memcpy(dstCb, &cb, sizeof(TrailInstanceCB));

        D3D12_GPU_VIRTUAL_ADDRESS cbVA = m_cbRing->GetGPUVirtualAddress() + i * cbStride;
        cmd->SetGraphicsRootConstantBufferView(TrailPipeline::SLOT_CB, cbVA);
        cmd->SetGraphicsRootDescriptorTable(TrailPipeline::SLOT_TEXTURE, getOrLoadTexture(tr.texturePath));

        cmd->DrawInstanced(vCount, 1, 0, 0);

        vertexCursor += vCount;
    }

    END_EVENT(cmd);
}
