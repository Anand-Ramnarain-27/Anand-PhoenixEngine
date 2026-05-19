#include "Globals.h"
#include "GBuffer.h"
#include "Application.h"
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
    static const Vector4 kClearColors[GBuffer::NUM_COLOR_RTS] = {
        { 0.f, 0.f, 0.f, 1.f },
        { 0.f, 0.f, 0.f, 0.f },
        { 0.f, 0.f, 0.f, 1.f }
    };
}

void GBuffer::resize(uint32_t w, uint32_t h) {
    if (m_width == w && m_height == h) return;
    release();
    if (w == 0 || h == 0) return;

    m_width  = w;
    m_height = h;

    auto* gpuRes = app->getGPUResources();
    auto* rtd    = app->getRTDescriptors();
    auto* dsd    = app->getDSDescriptors();
    auto* sd     = app->getShaderDescriptors();

    for (int i = 0; i < NUM_COLOR_RTS; ++i) {
        m_colorTextures[i] = gpuRes->createRenderTarget(
            kColorFormats[i], w, h, 1, kClearColors[i], kColorNames[i]);

        m_rtvDescs[i] = rtd->create(m_colorTextures[i].Get());

        m_srvTables[i] = sd->allocTable(kColorNames[i]);
        if (m_srvTables[i].isValid())
            m_srvTables[i].createTexture2DSRV(m_colorTextures[i].Get(), 0, kColorFormats[i]);
    }

    m_depthTexture = gpuRes->createDepthStencil(kDepthFormat, w, h, 1, 1.0f, 0, "GBuffer_Depth");
    m_dsvDesc = dsd->create(m_depthTexture.Get());
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
    m_width  = 0;
    m_height = 0;
}

void GBuffer::beginGeomPass(ID3D12GraphicsCommandList* cmd, bool clear) {
    if (!isValid()) return;

    // SRV -> RTV for all color targets
    CD3DX12_RESOURCE_BARRIER barriers[NUM_COLOR_RTS];
    for (int i = 0; i < NUM_COLOR_RTS; ++i) {
        barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_colorTextures[i].Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    cmd->ResourceBarrier(NUM_COLOR_RTS, barriers);

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
    D3D12_RECT     sc = { 0,   0,   LONG(m_width),  LONG(m_height) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);
}

void GBuffer::endGeomPass(ID3D12GraphicsCommandList* cmd) {
    if (!isValid()) return;

    // RTV -> SRV for all color targets
    CD3DX12_RESOURCE_BARRIER barriers[NUM_COLOR_RTS];
    for (int i = 0; i < NUM_COLOR_RTS; ++i) {
        barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_colorTextures[i].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    cmd->ResourceBarrier(NUM_COLOR_RTS, barriers);
}

D3D12_GPU_DESCRIPTOR_HANDLE GBuffer::getSrvHandle(Target t) const {
    return m_srvTables[static_cast<int>(t)].getGPUHandle(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE GBuffer::getDsvHandle() const {
    return m_dsvDesc.getCPUHandle();
}
