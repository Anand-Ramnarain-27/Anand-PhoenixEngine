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

    if (m_width == 0 || m_height == 0) {
        releaseResources();
        return;
    }

    ModuleResources* resources = app->getResources();
    ModuleShaderDescriptors* shaderDescriptors = app->getShaderDescriptors();
    ModuleRTDescriptors* rtDescriptors = app->getRTDescriptors();

    resources->deferRelease(m_textures.texture);
    m_textures.texture = resources->createRenderTarget(m_format, static_cast<size_t>(m_width), static_cast<size_t>(m_height), m_msaa ? 4 : 1, m_clearColor, m_name);

    if (m_autoResolveMSAA && m_msaa) {
        resources->deferRelease(m_textures.resolved);
        m_textures.resolved = resources->createRenderTarget(m_format,
            static_cast<size_t>(m_width), static_cast<size_t>(m_height),
            1, m_clearColor, (std::string(m_name) + "_resolved").c_str());
    }

    m_rtvDesc = rtDescriptors->create(m_textures.texture.Get());

    if (m_srvDesc.isValid()) {
        m_srvDesc.reset();
    }

    m_srvDesc = shaderDescriptors->allocTable(m_name);
    ID3D12Resource* srvResource = m_textures.resolved ? m_textures.resolved.Get() : m_textures.texture.Get();
    if (m_srvDesc.isValid()) {
        m_srvDesc.createTexture2DSRV(srvResource, 0);
    }

    if (m_depthFormat != DXGI_FORMAT_UNKNOWN) {
        ModuleDSDescriptors* dsDescriptors = app->getDSDescriptors();

        resources->deferRelease(m_textures.depthTexture);
        m_textures.depthTexture = resources->createDepthStencil(m_depthFormat,
            static_cast<size_t>(m_width), static_cast<size_t>(m_height),
            m_msaa ? 4 : 1, m_clearDepth, 0, m_name);

        m_dsvDesc = dsDescriptors->create(m_textures.depthTexture.Get());
    }

    m_handlesValid = false;
}

void RenderTexture::releaseResources()
{
    ModuleResources* resources = app->getResources();

    if (m_textures.texture) resources->deferRelease(m_textures.texture);
    if (m_textures.resolved) resources->deferRelease(m_textures.resolved);
    if (m_textures.depthTexture) resources->deferRelease(m_textures.depthTexture);

    m_srvDesc.reset();
    m_rtvDesc.reset();
    m_dsvDesc.reset();

    m_textures = {};
    m_handlesValid = false;
}

void RenderTexture::updateCachedHandles() const
{
    if (m_handlesValid) return;

    if (m_rtvDesc) {
        m_cachedRTV = m_rtvDesc.getCPUHandle();
    }

    if (m_dsvDesc) {
        m_cachedDSV = m_dsvDesc.getCPUHandle();
    }

    if (m_srvDesc.isValid()) {
        m_cachedSRV = m_srvDesc.getGPUHandle();
    }

    m_handlesValid = true;
}

void RenderTexture::beginRender(ID3D12GraphicsCommandList* cmdList, bool clear)
{
    if (!isValid()) return;

    CD3DX12_RESOURCE_BARRIER toRT = CD3DX12_RESOURCE_BARRIER::Transition(
        m_textures.texture.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    cmdList->ResourceBarrier(1, &toRT);

    updateCachedHandles();

    if (m_depthFormat != DXGI_FORMAT_UNKNOWN) {
        cmdList->OMSetRenderTargets(1, &m_cachedRTV, FALSE, &m_cachedDSV);
        if (clear) {
            cmdList->ClearRenderTargetView(m_cachedRTV, &m_clearColor.x, 0, nullptr);
            cmdList->ClearDepthStencilView(m_cachedDSV,
                D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                m_clearDepth, 0, 0, nullptr);
        }
    }
    else {
        cmdList->OMSetRenderTargets(1, &m_cachedRTV, FALSE, nullptr);
        if (clear) {
            cmdList->ClearRenderTargetView(m_cachedRTV, &m_clearColor.x, 0, nullptr);
        }
    }

    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, float(m_width), float(m_height), 0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0, LONG(m_width), LONG(m_height) };
    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissor);
}

void RenderTexture::endRender(ID3D12GraphicsCommandList* cmdList)
{
    if (!isValid()) return;

    if (m_msaa && m_autoResolveMSAA && m_textures.resolved) {
        resolveMSAA(cmdList);
    }
    else {
        CD3DX12_RESOURCE_BARRIER toSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_textures.texture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmdList->ResourceBarrier(1, &toSRV);
    }
}

void RenderTexture::bindAsShaderResource(ID3D12GraphicsCommandList* cmdList, int slot)
{
    if (!isValid()) return;

    updateCachedHandles();
    cmdList->SetGraphicsRootDescriptorTable(slot, m_cachedSRV);
}

void RenderTexture::resolveMSAA(ID3D12GraphicsCommandList* cmdList)
{
    if (!m_textures.resolved) return;

    CD3DX12_RESOURCE_BARRIER toResolveSrc = CD3DX12_RESOURCE_BARRIER::Transition(m_textures.texture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    cmdList->ResourceBarrier(1, &toResolveSrc);

    CD3DX12_RESOURCE_BARRIER toResolveDst = CD3DX12_RESOURCE_BARRIER::Transition(m_textures.resolved.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST);
    cmdList->ResourceBarrier(1, &toResolveDst);

    cmdList->ResolveSubresource(m_textures.resolved.Get(), 0, m_textures.texture.Get(), 0, m_format);

    CD3DX12_RESOURCE_BARRIER backToRTV = CD3DX12_RESOURCE_BARRIER::Transition(m_textures.texture.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(1, &backToRTV);

    CD3DX12_RESOURCE_BARRIER backToSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_textures.resolved.Get(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(1, &backToSRV);
}