#include "Globals.h"
#include "RenderTexture.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleRTDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleD3D12.h"

void RenderTexture::resize(UINT width, UINT height)
{
    if (m_width == width && m_height == height) return;
    m_width = width;
    m_height = height;

    if (m_width == 0 || m_height == 0) { releaseResources(); return; }

    auto* resources = app->getResources();
    auto* shaderDesc = app->getShaderDescriptors();
    auto* rtDesc = app->getRTDescriptors();

    resources->deferRelease(m_textures.texture);
    m_textures.texture = resources->createRenderTarget(
        m_format, (size_t)m_width, (size_t)m_height, m_msaa ? 4 : 1, m_clearColor, m_name);

    if (m_autoResolveMSAA && m_msaa)
    {
        resources->deferRelease(m_textures.resolved);
        m_textures.resolved = resources->createRenderTarget(
            m_format, (size_t)m_width, (size_t)m_height, 1, m_clearColor,
            (std::string(m_name) + "_resolved").c_str());
    }

    m_rtvDesc = rtDesc->create(m_textures.texture.Get());

    if (m_srvDesc.isValid()) m_srvDesc.reset();
    m_srvDesc = shaderDesc->allocTable(m_name);

    ID3D12Resource* srvRes = m_textures.resolved ? m_textures.resolved.Get() : m_textures.texture.Get();
    if (m_srvDesc.isValid()) m_srvDesc.createTexture2DSRV(srvRes, 0);

    if (m_depthFormat != DXGI_FORMAT_UNKNOWN)
    {
        resources->deferRelease(m_textures.depthTexture);
        m_textures.depthTexture = resources->createDepthStencil(
            m_depthFormat, (size_t)m_width, (size_t)m_height, m_msaa ? 4 : 1, m_clearDepth, 0, m_name);
        m_dsvDesc = app->getDSDescriptors()->create(m_textures.depthTexture.Get());
    }

    m_handlesValid = false;
}

void RenderTexture::releaseResources()
{
    auto* resources = app->getResources();
    if (m_textures.texture)      resources->deferRelease(m_textures.texture);
    if (m_textures.resolved)     resources->deferRelease(m_textures.resolved);
    if (m_textures.depthTexture) resources->deferRelease(m_textures.depthTexture);
    m_srvDesc.reset();
    m_rtvDesc.reset();
    m_dsvDesc.reset();
    m_textures = {};
    m_handlesValid = false;
}

void RenderTexture::updateCachedHandles() const
{
    if (m_rtvDesc)           m_cachedRTV = m_rtvDesc.getCPUHandle();
    if (m_dsvDesc)           m_cachedDSV = m_dsvDesc.getCPUHandle();
    if (m_srvDesc.isValid()) m_cachedSRV = m_srvDesc.getGPUHandle();
    m_handlesValid = true;
}

void RenderTexture::beginRender(ID3D12GraphicsCommandList* cmdList, bool clear)
{
    if (!isValid()) return;

    auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(m_textures.texture.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ResourceBarrier(1, &toRT);

    ensureHandles();

    const bool hasDepth = m_depthFormat != DXGI_FORMAT_UNKNOWN;
    cmdList->OMSetRenderTargets(1, &m_cachedRTV, FALSE, hasDepth ? &m_cachedDSV : nullptr);

    if (clear)
    {
        cmdList->ClearRenderTargetView(m_cachedRTV, &m_clearColor.x, 0, nullptr);
        if (hasDepth)
            cmdList->ClearDepthStencilView(m_cachedDSV,
                D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, m_clearDepth, 0, 0, nullptr);
    }

    D3D12_VIEWPORT vp{ 0.f, 0.f, float(m_width), float(m_height), 0.f, 1.f };
    D3D12_RECT     sc{ 0, 0, LONG(m_width), LONG(m_height) };
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &sc);
}

void RenderTexture::endRender(ID3D12GraphicsCommandList* cmdList)
{
    if (!isValid()) return;

    if (m_msaa && m_autoResolveMSAA && m_textures.resolved)
    {
        resolveMSAA(cmdList);
    }
    else
    {
        auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_textures.texture.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmdList->ResourceBarrier(1, &toSRV);
    }
}

void RenderTexture::bindAsShaderResource(ID3D12GraphicsCommandList* cmdList, int slot)
{
    if (!isValid()) return;
    ensureHandles();
    cmdList->SetGraphicsRootDescriptorTable(slot, m_cachedSRV);
}

void RenderTexture::resolveMSAA(ID3D12GraphicsCommandList* cmdList)
{
    if (!m_textures.resolved) return;

    CD3DX12_RESOURCE_BARRIER barriers[2] = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_textures.texture.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,        D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(m_textures.resolved.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST),
    };
    cmdList->ResourceBarrier(2, barriers);

    cmdList->ResolveSubresource(m_textures.resolved.Get(), 0, m_textures.texture.Get(), 0, m_format);

    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_textures.texture.Get(),
        D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_textures.resolved.Get(),
        D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(2, barriers);
}