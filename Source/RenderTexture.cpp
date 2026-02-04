// RenderTexture.cpp
#include "Globals.h"
#include "RenderTexture.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleRTDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleD3D12.h"

RenderTexture::~RenderTexture()
{
    releaseResources();
}

void RenderTexture::resize(UINT width, UINT height)
{
    if (m_width == width && m_height == height) return;

    m_width = width;
    m_height = height;

    if (m_width == 0 || m_height == 0) {
        releaseResources();
        return;
    }

    // Release old resources
    releaseResources();

    // Create new resources
    createResources();
}

void RenderTexture::createResources()
{
    ModuleResources* resources = app->getResources();

    // Create main render target
    m_textures.texture = resources->createRenderTarget(
        m_format, m_width, m_height, m_msaa ? 4 : 1, m_clearColor, m_name
    );

    // Create MSAA resolve target if needed
    if (m_autoResolveMSAA && m_msaa) {
        m_textures.resolved = resources->createRenderTarget(
            m_format, m_width, m_height, 1, m_clearColor,
            (std::string(m_name) + "_resolved").c_str()
        );
    }

    // Create RTV
    ModuleRTDescriptors* rtDescriptors = app->getRTDescriptors();
    m_rtvDesc = rtDescriptors->create(m_textures.texture.Get());

    // Create SRV - using shared_ptr as per ModuleShaderDescriptors::allocTable()
    ModuleShaderDescriptors* shaderDescriptors = app->getShaderDescriptors();
    m_srvDesc = shaderDescriptors->allocTable(m_name);

    ID3D12Resource* srvResource = m_textures.resolved ? m_textures.resolved.Get() : m_textures.texture.Get();
    if (m_srvDesc) {
        m_srvDesc->createTexture2DSRV(0, srvResource);
    }

    // Create depth if needed
    if (m_depthFormat != DXGI_FORMAT_UNKNOWN) {
        ModuleDSDescriptors* dsDescriptors = app->getDSDescriptors();

        m_textures.depthTexture = resources->createDepthStencil(
            m_depthFormat, m_width, m_height, m_msaa ? 4 : 1,
            m_clearDepth, 0, m_name
        );

        m_dsvDesc = dsDescriptors->create(m_textures.depthTexture.Get());
    }

    m_handlesValid = false;
}

void RenderTexture::releaseResources()
{
    ModuleResources* resources = app->getResources();

    // Defer release to ensure GPU safety
    if (m_textures.texture) resources->deferRelease(m_textures.texture);
    if (m_textures.resolved) resources->deferRelease(m_textures.resolved);
    if (m_textures.depthTexture) resources->deferRelease(m_textures.depthTexture);

    m_textures = {};

    // Clear descriptor handles
    m_rtvDesc.reset();
    m_dsvDesc.reset();
    m_srvDesc.reset();

    m_handlesValid = false;
}

void RenderTexture::updateCachedHandles() const
{
    if (m_handlesValid) return;

    m_cachedRTV = m_rtvDesc.getCPUHandle();
    m_cachedDSV = m_dsvDesc.getCPUHandle();

    if (m_srvDesc && m_srvDesc->isValid()) {
        m_cachedSRV = m_srvDesc->getGPUHandle();
    }
    else {
        m_cachedSRV = {};
    }

    m_handlesValid = true;
}

void RenderTexture::beginRender(ID3D12GraphicsCommandList* cmdList, bool clear)
{
    if (!isValid()) return;

    // Transition to RENDER_TARGET
    transition(cmdList,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        m_textures.texture.Get());

    // Set render target
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = getRtvHandle();

    if (m_depthFormat != DXGI_FORMAT_UNKNOWN) {
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = getDsvHandle();
        cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        if (clear) {
            cmdList->ClearRenderTargetView(rtv, &m_clearColor.x, 0, nullptr);
            cmdList->ClearDepthStencilView(dsv,
                D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                m_clearDepth, 0, 0, nullptr);
        }
    }
    else {
        cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        if (clear) {
            cmdList->ClearRenderTargetView(rtv, &m_clearColor.x, 0, nullptr);
        }
    }

    // Set viewport and scissor
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
        // Transition back to SHADER_RESOURCE
        transition(cmdList,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            m_textures.texture.Get());
    }
}

void RenderTexture::bindAsShaderResource(ID3D12GraphicsCommandList* cmdList, int slot)
{
    if (!isValid()) return;

    D3D12_GPU_DESCRIPTOR_HANDLE srv = getSrvHandle();
    cmdList->SetGraphicsRootDescriptorTable(slot, srv);
}

void RenderTexture::resolveMSAA(ID3D12GraphicsCommandList* cmdList)
{
    if (!m_textures.resolved) return;

    // Transition both textures for resolve
    transition(cmdList,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
        m_textures.texture.Get());

    transition(cmdList,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RESOLVE_DEST,
        m_textures.resolved.Get());

    // Perform resolve
    cmdList->ResolveSubresource(
        m_textures.resolved.Get(), 0,
        m_textures.texture.Get(), 0,
        m_format
    );

    // Transition back
    transition(cmdList,
        D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        m_textures.texture.Get());

    transition(cmdList,
        D3D12_RESOURCE_STATE_RESOLVE_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        m_textures.resolved.Get());
}

void RenderTexture::transition(ID3D12GraphicsCommandList* cmdList,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after,
    ID3D12Resource* resource)
{
    if (!resource) resource = m_textures.texture.Get();

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        resource, before, after
    );
    cmdList->ResourceBarrier(1, &barrier);
}