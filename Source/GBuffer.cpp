#include "Globals.h"
#include "GBuffer.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleGPUResources.h"
#include "ModuleRTDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleShaderDescriptors.h"
#include <d3dx12.h>

namespace {
    static constexpr DXGI_FORMAT kColorFormats[GBuffer::NUM_COLOR_RTS] = {
        GBuffer::kAlbedoFormat,
        GBuffer::kNormalMetalRoughFormat,
        GBuffer::kEmissiveAOFormat
    };
    static constexpr const char* kColorNames[GBuffer::NUM_COLOR_RTS] = {
        "GBuffer_Albedo",
        "GBuffer_NormalMetalRough",
        "GBuffer_EmissiveAO"
    };
    // Albedo alpha=0 so the deferred lighting pass can detect background pixels (no geometry).
    // Geometry always writes alpha=1 in GBufferPS.
    static const Vector4 kClearColors[GBuffer::NUM_COLOR_RTS] = {
        { 0.f, 0.f, 0.f, 0.f },
        { 0.f, 0.f, 0.f, 0.f },
        { 0.f, 0.f, 0.f, 1.f }
    };
}

void GBuffer::resize(uint32_t w, uint32_t h) {
    if (m_width == w && m_height == h) return;
    release();
    if (w == 0 || h == 0) return;

    m_width = w;
    m_height = h;

    auto* gpuRes = app->getGPUResources();
    auto* rtd = app->getRTDescriptors();
    auto* dsd = app->getDSDescriptors();
    auto* sd = app->getShaderDescriptors();

    for (int i = 0; i < NUM_COLOR_RTS; ++i) {
        m_colorTextures[i] = gpuRes->createRenderTarget(
            kColorFormats[i], w, h, 1, kClearColors[i], kColorNames[i]);

        m_rtvDescs[i] = rtd->create(m_colorTextures[i].Get());

        m_srvTables[i] = sd->allocTable(kColorNames[i]);
        if (m_srvTables[i].isValid())
            m_srvTables[i].createTexture2DSRV(m_colorTextures[i].Get(), 0, kColorFormats[i]);
    }

    // Create depth as R32_TYPELESS so we can bind it both as DSV and as an R32_FLOAT SRV
    {
        D3D12_RESOURCE_DESC dd = {};
        dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        dd.Width = w;
        dd.Height = h;
        dd.DepthOrArraySize = 1;
        dd.MipLevels = 1;
        dd.Format = DXGI_FORMAT_R32_TYPELESS;
        dd.SampleDesc = { 1, 0 };
        dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_CLEAR_VALUE cv = {};
        cv.Format = DXGI_FORMAT_D32_FLOAT;
        cv.DepthStencil.Depth = 1.0f;
        app->getD3D12()->getDevice()->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &dd,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS(&m_depthTexture));
        m_depthTexture->SetName(L"GBuffer_Depth");
    }

    // DSV with concrete D32_FLOAT format (writable)
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_dsvDesc = dsd->create(m_depthTexture.Get(), &dsvDesc);

    // Read-only DSV — used by the transparent forward pass (depth test, no depth write)
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvRODesc = {};
    dsvRODesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvRODesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvRODesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
    m_dsvReadOnly = dsd->create(m_depthTexture.Get(), &dsvRODesc);

    // SRV for sampling depth as R32_FLOAT in the deferred lighting pass
    m_depthSrvTable = sd->allocTable("GBuffer_DepthSRV");
    if (m_depthSrvTable.isValid())
        m_depthSrvTable.createTexture2DSRV(m_depthTexture.Get(), 0, DXGI_FORMAT_R32_FLOAT, 1);

    m_depthReadable = false;
}

void GBuffer::release() {
    auto* gpuRes = app->getGPUResources();
    for (int i = 0; i < NUM_COLOR_RTS; ++i) {
        if (m_colorTextures[i]) gpuRes->deferRelease(m_colorTextures[i]);
        m_rtvDescs[i].reset();
        m_srvTables[i].reset();
    }
    if (m_depthTexture) gpuRes->deferRelease(m_depthTexture);
    m_dsvDesc.reset();
    m_dsvReadOnly.reset();
    m_depthSrvTable.reset();
    m_depthReadable = false;
    m_width = 0;
    m_height = 0;
}

void GBuffer::beginGeomPass(ID3D12GraphicsCommandList* cmd, bool clear) {
    if (!isValid()) return;

    // SRV -> RTV for all color targets; also transition depth back from readable state if needed
    CD3DX12_RESOURCE_BARRIER barriers[NUM_COLOR_RTS + 1];
    int barrierCount = NUM_COLOR_RTS;
    for (int i = 0; i < NUM_COLOR_RTS; ++i) {
        barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_colorTextures[i].Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    if (m_depthReadable) {
        barriers[barrierCount++] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_depthTexture.Get(),
            D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        m_depthReadable = false;
    }
    cmd->ResourceBarrier(barrierCount, barriers);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[NUM_COLOR_RTS];
    for (int i = 0; i < NUM_COLOR_RTS; ++i)
        rtvHandles[i] = m_rtvDescs[i].getCPUHandle();

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvDesc.getCPUHandle();
    cmd->OMSetRenderTargets(NUM_COLOR_RTS, rtvHandles, FALSE, &dsvHandle);

    if (clear) {
        for (int i = 0; i < NUM_COLOR_RTS; ++i)
            cmd->ClearRenderTargetView(rtvHandles[i], &kClearColors[i].x, 0, nullptr);
        cmd->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }

    D3D12_VIEWPORT vp = { 0.f, 0.f, float(m_width), float(m_height), 0.f, 1.f };
    D3D12_RECT sc = { 0, 0, LONG(m_width), LONG(m_height) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);
}

void GBuffer::endGeomPass(ID3D12GraphicsCommandList* cmd) {
    if (!isValid()) return;

    // RTV -> SRV for all color targets; depth DEPTH_WRITE -> DEPTH_READ|PSR for sampling
    CD3DX12_RESOURCE_BARRIER barriers[NUM_COLOR_RTS + 1];
    for (int i = 0; i < NUM_COLOR_RTS; ++i) {
        barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_colorTextures[i].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    barriers[NUM_COLOR_RTS] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_depthTexture.Get(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_depthReadable = true;
    cmd->ResourceBarrier(NUM_COLOR_RTS + 1, barriers);
}

D3D12_GPU_DESCRIPTOR_HANDLE GBuffer::getSrvHandle(Target t) const {
    return m_srvTables[static_cast<int>(t)].getGPUHandle(0);
}

D3D12_GPU_DESCRIPTOR_HANDLE GBuffer::getDepthSrvHandle() const {
    return m_depthSrvTable.getGPUHandle(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE GBuffer::getDsvHandle() const {
    return m_dsvDesc.getCPUHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE GBuffer::getReadOnlyDsvHandle() const {
    return m_dsvReadOnly.getCPUHandle();
}
