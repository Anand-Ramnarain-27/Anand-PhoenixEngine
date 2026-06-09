#include "Globals.h"
#include "BillboardPass.h"
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

bool BillboardPass::init(ID3D12Device* device){
    if (!m_pipeline.init(device)) {
        LOG("BillboardPass: pipeline init failed");
        return false;
    }
    if (!createUploadBuffer(device)) return false;
    if (!createFallbackTexture(device)) return false;

    LOG("BillboardPass: init OK");
    if (auto* ed = app->getEditor())
        ed->log("BillboardPass: initialized OK", ImVec4(0.5f, 1.f, 0.5f, 1.f));
    return true;
}

bool BillboardPass::createUploadBuffer(ID3D12Device* device){
    const UINT stride = cbAlign(sizeof(BillboardInstanceCB));
    const UINT64 total = stride * MAX_BILLBOARDS;
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bd = CD3DX12_RESOURCE_DESC::Buffer(total);
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
                                                  nullptr, IID_PPV_ARGS(&m_cbRing));
    if (FAILED(hr)) { LOG("BillboardPass: CB ring alloc failed 0x%08X", hr); return false; }
    m_cbRing->SetName(L"Billboard_CBRing");
    m_cbRing->Map(0, nullptr, &m_cbMapped);
    return true;
}

bool BillboardPass::createFallbackTexture(ID3D12Device* device){
    (void)device; // resource creation now goes through ModuleGPUResources
    // Upload actual opaque-white pixel data — a committed resource with no
    // initial data holds undefined GPU memory, which previously showed up as
    // a solid garbage-coloured quad whenever a billboard's texture failed to load.
    const uint32_t white = 0xFFFFFFFFu;
    m_fallbackTex = app->getGPUResources()->createRawTexture2D(&white, sizeof(white), 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!m_fallbackTex) {
        LOG("BillboardPass: fallback texture creation failed");
        return false;
    }
    m_fallbackTex->SetName(L"Billboard_FallbackTex");

    auto* sd = app->getShaderDescriptors();
    m_fallbackSRV = sd->allocTable("Billboard_FallbackSRV");
    if (!m_fallbackSRV.isValid()) {
        LOG("BillboardPass: fallback SRV alloc failed");
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

D3D12_GPU_DESCRIPTOR_HANDLE BillboardPass::getOrLoadTexture(const std::string& path){
    if (path.empty())
        return m_fallbackSRV.getGPUHandle(0);

    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end())
        return it->second.srv.getGPUHandle(0);

    auto* gpu = app->getGPUResources();
    ComPtr<ID3D12Resource> tex = gpu ? gpu->createTextureFromFile(path, true) : nullptr;
    if (!tex) {
        LOG("BillboardPass: failed to load texture '%s', using fallback", path.c_str());
        if (auto* ed = app->getEditor())
            ed->log(("Billboard: failed to load texture '" + path + "' (using fallback)").c_str(), ImVec4(1.f, 0.4f, 0.4f, 1.f));
        // Cache the failure as a fallback entry so we don't retry every frame.
        m_textureCache.emplace(path, CachedTexture{ nullptr, m_fallbackSRV });
        return m_fallbackSRV.getGPUHandle(0);
    }

    ShaderTableDesc srv = app->getShaderDescriptors()->allocTable(("Billboard_SRV_" + path).c_str());
    if (!srv.isValid()) {
        LOG("BillboardPass: SRV alloc failed for '%s', using fallback", path.c_str());
        if (auto* ed = app->getEditor())
            ed->log(("Billboard: SRV alloc failed for '" + path + "' (using fallback)").c_str(), ImVec4(1.f, 0.4f, 0.4f, 1.f));
        m_textureCache.emplace(path, CachedTexture{ nullptr, m_fallbackSRV });
        return m_fallbackSRV.getGPUHandle(0);
    }
    srv.createTexture2DSRV(tex.Get(), 0);

    if (auto* ed = app->getEditor()) {
        D3D12_RESOURCE_DESC rd = tex->GetDesc();
        ed->log(("Billboard: loaded texture '" + path + "' (" + std::to_string(rd.Width) + "x" +
                 std::to_string(rd.Height) + ", fmt " + std::to_string((int)rd.Format) + ")").c_str(),
                ImVec4(0.5f, 1.f, 0.5f, 1.f));
    }

    auto handle = srv.getGPUHandle(0);
    m_textureCache.emplace(path, CachedTexture{ std::move(tex), std::move(srv) });
    return handle;
}

void BillboardPass::render(ID3D12GraphicsCommandList* cmd,
                            const std::vector<BillboardInstance>& billboards,
                            uint32_t width, uint32_t height){
    if (billboards.empty() || width == 0 || height == 0) return;

    BEGIN_EVENT(cmd, L"Billboard Pass");

    cmd->SetGraphicsRootSignature(m_pipeline.getRootSig());

    ID3D12DescriptorHeap* heaps[] = {
        app->getShaderDescriptors()->getHeap(),
        app->getSamplerHeap()->getHeap()
    };
    cmd->SetDescriptorHeaps(2, heaps);

    cmd->SetGraphicsRootDescriptorTable(BillboardPipeline::SLOT_SAMPLER,
        app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->IASetIndexBuffer(nullptr);

    const UINT cbStride = cbAlign(sizeof(BillboardInstanceCB));
    const UINT count = std::min((UINT)billboards.size(), MAX_BILLBOARDS);

    bool additiveBound = false;
    cmd->SetPipelineState(m_pipeline.getPSO());

    for (UINT i = 0; i < count; ++i) {
        const BillboardInstance& bb = billboards[i];

        // Switch PSO only when the blend mode actually changes between consecutive
        // draws — instances are sorted back-to-front so runs are usually contiguous.
        if (bb.additive != additiveBound) {
            additiveBound = bb.additive;
            cmd->SetPipelineState(additiveBound ? m_pipeline.getAdditivePSO() : m_pipeline.getPSO());
        }

        void* dst = reinterpret_cast<uint8_t*>(m_cbMapped) + i * cbStride;
        memcpy(dst, &bb.cb, sizeof(BillboardInstanceCB));

        D3D12_GPU_VIRTUAL_ADDRESS cbVA = m_cbRing->GetGPUVirtualAddress() + i * cbStride;
        cmd->SetGraphicsRootConstantBufferView(BillboardPipeline::SLOT_CB, cbVA);
        cmd->SetGraphicsRootDescriptorTable(BillboardPipeline::SLOT_TEXTURE,
                                             getOrLoadTexture(bb.texturePath));

        cmd->DrawInstanced(4, 1, 0, 0);
    }

    END_EVENT(cmd);
}
