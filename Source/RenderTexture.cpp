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

    // Update size
    m_width = width;
    m_height = height;

    // Release old resources if size is zero
    if (m_width == 0 || m_height == 0) {
        releaseResources();
        return;
    }

    ModuleResources* resources = app->getResources();
    ModuleShaderDescriptors* shaderDescriptors = app->getShaderDescriptors();
    ModuleRTDescriptors* rtDescriptors = app->getRTDescriptors();

    // --- Create color texture ---
    resources->deferRelease(m_textures.texture);
    m_textures.texture = resources->createRenderTarget(
        m_format,
        static_cast<size_t>(m_width),
        static_cast<size_t>(m_height),
        m_msaa ? 4 : 1,
        m_clearColor,
        m_name
    );

    // --- Create MSAA resolve texture if needed ---
    if (m_autoResolveMSAA && m_msaa) {
        resources->deferRelease(m_textures.resolved);
        m_textures.resolved = resources->createRenderTarget(
            m_format,
            static_cast<size_t>(m_width),
            static_cast<size_t>(m_height),
            1,
            m_clearColor,
            (std::string(m_name) + "_resolved").c_str()
        );
    }

    // --- Create RTV descriptor ---
    m_rtvDesc = rtDescriptors->create(m_textures.texture.Get());

    // --- Create SRV safely ---
    if (m_srvDesc.isValid()) m_srvDesc.reset(); // reset old table
    m_srvDesc = shaderDescriptors->allocTable(m_name);

    // Ensure table has at least one descriptor before writing
    if (!m_srvDesc.isValid() || m_srvDesc.numDescriptors() == 0) {
        // Allocate 1 descriptor if none exists
        m_srvDesc.allocate(1); // <-- SAFE: vector now has element 0
    }

    // Use resolved texture if available, otherwise normal texture
    ID3D12Resource* srvResource = (m_autoResolveMSAA && m_msaa && m_textures.resolved)
        ? m_textures.resolved.Get()
        : m_textures.texture.Get();

    // Safe: vector guaranteed to have element 0
    m_srvDesc.createTexture2DSRV(srvResource, 0);

    // --- Create depth texture if needed ---
    if (m_depthFormat != DXGI_FORMAT_UNKNOWN) {
        ModuleDSDescriptors* dsDescriptors = app->getDSDescriptors();

        resources->deferRelease(m_textures.depthTexture);
        m_textures.depthTexture = resources->createDepthStencil(
            m_depthFormat,
            static_cast<size_t>(m_width),
            static_cast<size_t>(m_height),
            m_msaa ? 4 : 1,
            m_clearDepth,
            0,
            m_name
        );

        m_dsvDesc = dsDescriptors->create(m_textures.depthTexture.Get());
    }

    // Mark handles invalid for lazy caching
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

void RenderTexture::transition(ID3D12GraphicsCommandList* cmdList,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after,
    ID3D12Resource* resource)
{
    if (!resource) return;
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after);
    cmdList->ResourceBarrier(1, &barrier);
}

void RenderTexture::updateCachedHandles() const
{
    if (m_handlesValid) return;

    auto cache = [&](auto& desc, auto& cached) {
        if (desc) cached = desc.getCPUHandle();
        };

    cache(m_rtvDesc, m_cachedRTV);
    cache(m_dsvDesc, m_cachedDSV);
    m_cachedSRV = m_srvDesc.isValid() ? m_srvDesc.getGPUHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{};
    m_handlesValid = true;
}

void RenderTexture::beginRender(ID3D12GraphicsCommandList* cmdList, bool clear)
{
    if (!isValid()) return;

    transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, m_textures.texture.Get());
    updateCachedHandles();

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_depthFormat != DXGI_FORMAT_UNKNOWN ? m_cachedDSV : D3D12_CPU_DESCRIPTOR_HANDLE{};
    cmdList->OMSetRenderTargets(1, &m_cachedRTV, FALSE, m_depthFormat != DXGI_FORMAT_UNKNOWN ? &dsv : nullptr);

    if (clear) {
        cmdList->ClearRenderTargetView(m_cachedRTV, &m_clearColor.x, 0, nullptr);
        if (m_depthFormat != DXGI_FORMAT_UNKNOWN)
            cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, m_clearDepth, 0, 0, nullptr);
    }

    D3D12_VIEWPORT vp{ 0,0, float(m_width), float(m_height), 0.0f, 1.0f };
    D3D12_RECT sc{ 0,0, LONG(m_width), LONG(m_height) };
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &sc);
}

void RenderTexture::endRender(ID3D12GraphicsCommandList* cmdList)
{
    if (!isValid()) return;

    if (m_msaa && m_autoResolveMSAA && m_textures.resolved)
        resolveMSAA(cmdList);
    else
        transition(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, m_textures.texture.Get());
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

    transition(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, m_textures.texture.Get());
    transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST, m_textures.resolved.Get());

    cmdList->ResolveSubresource(m_textures.resolved.Get(), 0, m_textures.texture.Get(), 0, m_format);

    transition(cmdList, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, m_textures.texture.Get());
    transition(cmdList, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, m_textures.resolved.Get());
}