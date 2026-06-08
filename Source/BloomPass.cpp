#include "Globals.h"
#include "BloomPass.h"
#include "RenderTexture.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleGPUResources.h"
#include <d3dx12.h>
#include <cstring>

// Bloom intermediate format: R16G16B16A16_FLOAT is UAV-compatible (unlike sRGB formats)
static constexpr DXGI_FORMAT kBloomFmt = DXGI_FORMAT_R16G16B16A16_FLOAT;

// Thread-group sizes (must match [numthreads] in the shaders)
static constexpr UINT kThreshGroupSize = 8;
static constexpr UINT kBlurGroupSize   = 128;

static constexpr UINT cbAlign(UINT b) { return (b + 255u) & ~255u; }

bool BloomPass::init(ID3D12Device* device, DXGI_FORMAT sceneRTFormat) {
    if (!m_pipeline.init(device, sceneRTFormat)) {
        LOG("BloomPass: pipeline init failed");
        return false;
    }
    // Constant buffer (16 bytes, aligned to 256)
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bd = CD3DX12_RESOURCE_DESC::Buffer(cbAlign(sizeof(CbBloom)));
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
                                                  nullptr, IID_PPV_ARGS(&m_cb));
    if (FAILED(hr)) { LOG("BloomPass: CB alloc failed 0x%08X", hr); return false; }
    m_cb->SetName(L"Bloom_CB");
    m_cb->Map(0, nullptr, &m_cbMapped);
    LOG("BloomPass: init OK");
    return true;
}

bool BloomPass::createIntermediateTextures(ID3D12Device* device, uint32_t w, uint32_t h) {
    releaseIntermediateTextures();

    auto createUAVTex = [&](ComPtr<ID3D12Resource>& tex, const wchar_t* name) -> bool {
        D3D12_RESOURCE_DESC d = {};
        d.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width               = w;
        d.Height              = h;
        d.DepthOrArraySize    = 1;
        d.MipLevels           = 1;
        d.Format              = kBloomFmt;
        d.SampleDesc          = { 1, 0 };
        // ALLOW_UNORDERED_ACCESS is required for UAV creation (lecture: resource must have this flag)
        d.Flags               = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d,
                                                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                                      nullptr, IID_PPV_ARGS(&tex));
        if (FAILED(hr)) { LOG("BloomPass: tex alloc failed 0x%08X", hr); return false; }
        tex->SetName(name);
        return true;
    };

    if (!createUAVTex(m_texA, L"Bloom_TexA")) return false;
    if (!createUAVTex(m_texB, L"Bloom_TexB")) return false;

    auto* sd = app->getShaderDescriptors();
    m_texA_SRV = sd->allocTable("Bloom_TexA_SRV");
    m_texA_UAV = sd->allocTable("Bloom_TexA_UAV");
    m_texB_SRV = sd->allocTable("Bloom_TexB_SRV");
    m_texB_UAV = sd->allocTable("Bloom_TexB_UAV");

    // SRV descriptor
    auto makeSRV = [&](ShaderTableDesc& t, ID3D12Resource* r) {
        D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
        sv.Format                    = kBloomFmt;
        sv.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels       = 1;
        t.createSRV(r, 0, &sv);
    };
    // UAV descriptor (lecture: CreateUnorderedAccessView with RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    auto makeUAV = [&](ShaderTableDesc& t, ID3D12Resource* r) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uv = {};
        uv.Format             = kBloomFmt;
        uv.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE2D;
        uv.Texture2D.MipSlice = 0;
        t.createUAV(r, 0, &uv);
    };

    makeSRV(m_texA_SRV, m_texA.Get());
    makeUAV(m_texA_UAV, m_texA.Get());
    makeSRV(m_texB_SRV, m_texB.Get());
    makeUAV(m_texB_UAV, m_texB.Get());

    m_allocW = w;
    m_allocH = h;
    return true;
}

void BloomPass::releaseIntermediateTextures() {
    if (!m_texA) return;
    auto* gpuRes = app->getGPUResources();
    gpuRes->deferRelease(m_texA);
    gpuRes->deferRelease(m_texB);
    m_texA_SRV.reset(); m_texA_UAV.reset();
    m_texB_SRV.reset(); m_texB_UAV.reset();
    m_allocW = m_allocH = 0;
}

void BloomPass::uploadCB(uint32_t w, uint32_t h) {
    CbBloom cb;
    cb.texW      = w;
    cb.texH      = h;
    cb.threshold = m_settings.threshold;
    cb.strength  = m_settings.strength;
    memcpy(m_cbMapped, &cb, sizeof(cb));
}

void BloomPass::render(ID3D12GraphicsCommandList* cmd,
                        RenderTexture& sceneRT,
                        uint32_t width, uint32_t height) {
    if (!m_settings.enabled || width == 0 || height == 0) return;
    if (!sceneRT.isValid()) return;

    // (Re)allocate intermediate textures when viewport resizes
    if (width != m_allocW || height != m_allocH || !m_texA) {
        auto* device = app->getD3D12()->getDevice();
        if (!createIntermediateTextures(device, width, height)) return;
    }

    uploadCB(width, height);

    BEGIN_EVENT(cmd, L"Bloom Pass");

    // ── Transition scene RT: RTV → PIXEL_SHADER_RESOURCE (compute needs SRV read) ──
    {
        auto bar = CD3DX12_RESOURCE_BARRIER::Transition(
            sceneRT.getTexture(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &bar);
    }

    ID3D12DescriptorHeap* heaps[] = {
        app->getShaderDescriptors()->getHeap(),
        app->getSamplerHeap()->getHeap()
    };
    cmd->SetDescriptorHeaps(2, heaps);

    // ════════════════════════════════════════════════════════════
    //  PASS 1: THRESHOLD  (8×8 threads, group count = (w+7)/8 × (h+7)/8)
    // ════════════════════════════════════════════════════════════
    BEGIN_EVENT(cmd, L"Bloom Threshold");
    cmd->SetPipelineState(m_pipeline.getThresholdPSO());
    cmd->SetComputeRootSignature(m_pipeline.getComputeRootSig());
    cmd->SetComputeRootConstantBufferView(BloomPipeline::CS_SLOT_CB, m_cb->GetGPUVirtualAddress());
    cmd->SetComputeRootDescriptorTable(BloomPipeline::CS_SLOT_INPUT,  sceneRT.getSrvHandle());
    cmd->SetComputeRootDescriptorTable(BloomPipeline::CS_SLOT_OUTPUT, m_texA_UAV.getGPUHandle(0));
    cmd->SetComputeRootDescriptorTable(BloomPipeline::CS_SLOT_SAMPLER,
        app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));
    // Lecture formula: group_count = (domain_size + num_threads - 1) / num_threads
    cmd->Dispatch((width  + kThreshGroupSize - 1) / kThreshGroupSize,
                  (height + kThreshGroupSize - 1) / kThreshGroupSize, 1);
    END_EVENT(cmd);

    // UAV barrier: ensure all threshold writes to texA are visible before the next dispatch
    // (lecture: "write-after-write or read-after-write across dispatches needs UAV barrier")
    {
        auto bar = CD3DX12_RESOURCE_BARRIER::UAV(m_texA.Get());
        cmd->ResourceBarrier(1, &bar);
    }

    // ════════════════════════════════════════════════════════════
    //  PASS 2: HORIZONTAL BLUR (texA → texB, 128×1 threads)
    // ════════════════════════════════════════════════════════════
    // Transition texA: UAV → SRV so the CS can read it as a Texture2D
    {
        auto bar = CD3DX12_RESOURCE_BARRIER::Transition(
            m_texA.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &bar);
    }

    BEGIN_EVENT(cmd, L"Bloom Blur H");
    cmd->SetPipelineState(m_pipeline.getBlurHPSO());
    // Root sig already set (compute root sig is the same for all three CS passes)
    cmd->SetComputeRootDescriptorTable(BloomPipeline::CS_SLOT_INPUT,  m_texA_SRV.getGPUHandle(0));
    cmd->SetComputeRootDescriptorTable(BloomPipeline::CS_SLOT_OUTPUT, m_texB_UAV.getGPUHandle(0));
    cmd->Dispatch((width  + kBlurGroupSize - 1) / kBlurGroupSize, height, 1);
    END_EVENT(cmd);

    // UAV barrier on texB before the vertical pass reads it
    {
        auto bar = CD3DX12_RESOURCE_BARRIER::UAV(m_texB.Get());
        cmd->ResourceBarrier(1, &bar);
    }

    // ════════════════════════════════════════════════════════════
    //  PASS 3: VERTICAL BLUR (texB → texA, 1×128 threads)
    // ════════════════════════════════════════════════════════════
    // Transitions: texA SRV → UAV (output), texB UAV → SRV (input)
    {
        CD3DX12_RESOURCE_BARRIER bars[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_texA.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
            CD3DX12_RESOURCE_BARRIER::Transition(m_texB.Get(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        };
        cmd->ResourceBarrier(2, bars);
    }

    BEGIN_EVENT(cmd, L"Bloom Blur V");
    cmd->SetPipelineState(m_pipeline.getBlurVPSO());
    cmd->SetComputeRootDescriptorTable(BloomPipeline::CS_SLOT_INPUT,  m_texB_SRV.getGPUHandle(0));
    cmd->SetComputeRootDescriptorTable(BloomPipeline::CS_SLOT_OUTPUT, m_texA_UAV.getGPUHandle(0));
    cmd->Dispatch(width, (height + kBlurGroupSize - 1) / kBlurGroupSize, 1);
    END_EVENT(cmd);

    // ════════════════════════════════════════════════════════════
    //  PASS 4: COMPOSITE — additive blend bloom onto scene RT
    // ════════════════════════════════════════════════════════════
    // Transition texA (final bloom): UAV → SRV for graphics PS read
    // Transition scene RT: PSR → RTV for graphics output
    {
        CD3DX12_RESOURCE_BARRIER bars[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_texA.Get(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(sceneRT.getTexture(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET)
        };
        cmd->ResourceBarrier(2, bars);
    }

    BEGIN_EVENT(cmd, L"Bloom Composite");
    auto rtv = sceneRT.getRtvHandle();
    cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    D3D12_VIEWPORT vp = { 0.f, 0.f, float(width), float(height), 0.f, 1.f };
    D3D12_RECT     sc = { 0, 0, LONG(width), LONG(height) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    cmd->SetPipelineState(m_pipeline.getCompositePSO());
    cmd->SetGraphicsRootSignature(m_pipeline.getCompositeRootSig());
    cmd->SetGraphicsRootDescriptorTable(BloomPipeline::GFX_SLOT_BLOOM_SRV,
                                         m_texA_SRV.getGPUHandle(0));
    cmd->SetGraphicsRootDescriptorTable(BloomPipeline::GFX_SLOT_SAMPLER,
        app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->DrawInstanced(3, 1, 0, 0);
    END_EVENT(cmd);

    // Reset texB and texA back to UAV ready for next frame
    {
        CD3DX12_RESOURCE_BARRIER bars[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_texA.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
            CD3DX12_RESOURCE_BARRIER::Transition(m_texB.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        };
        cmd->ResourceBarrier(2, bars);
    }

    END_EVENT(cmd);
    // sceneRT is left in D3D12_RESOURCE_STATE_RENDER_TARGET — endRender() transitions it to PSR.
}
