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

void RenderTexture::resize(UINT newWidth, UINT newHeight)
{
    if (newWidth == width && newHeight == height)
        return;

    width = newWidth;
    height = newHeight;

    if (width == 0 || height == 0)
        return;

    ModuleResources* resources = app->getResources();

    if (texture)
        resources->deferRelease(texture);
    if (resolvedTexture)
        resources->deferRelease(resolvedTexture);
    if (depthTexture)
        resources->deferRelease(depthTexture);

    createResources();
    updateDescriptors();
}

void RenderTexture::createResources()
{
    ModuleResources* resources = app->getResources();

    texture = resources->createRenderTarget(
        format,
        width,
        height,
        msaaSampleCount,
        clearColor,
        name
    );

    if (enableMSAA && autoResolveMSAA)
    {
        resolvedTexture = resources->createRenderTarget(
            format,
            width,
            height,
            1,
            clearColor,
            (std::string(name) + "_Resolved").c_str()
        );
    }

    if (depthFormat != DXGI_FORMAT_UNKNOWN)
    {
        depthTexture = resources->createDepthStencil(
            depthFormat,
            width,
            height,
            msaaSampleCount,
            clearDepth,
            0,
            (std::string(name) + "_Depth").c_str()
        );
    }
}

void RenderTexture::updateDescriptors()
{
    auto* rtDescriptors = app->getRTDescriptors();
    auto* shaderDescriptors = app->getShaderDescriptors();
    auto* dsDescriptors = app->getDSDescriptors();

    rtvDesc = rtDescriptors->create(texture.Get());

    srvDesc = shaderDescriptors->createTable();
    srvDesc.createTexture2DSRV(
        getSRVTexture(),
        format,
        UINT_MAX, 
        0        
    );

    if (depthTexture)
    {
        dsvDesc = dsDescriptors->create(depthTexture.Get());
    }
}

void RenderTexture::releaseResources()
{
    texture.Reset();
    resolvedTexture.Reset();
    depthTexture.Reset();
}

void RenderTexture::beginRendering(ID3D12GraphicsCommandList* cmdList)
{
    transitionToRTV(cmdList);

    setRenderTarget(cmdList);
}

void RenderTexture::endRendering(ID3D12GraphicsCommandList* cmdList)
{
    if (enableMSAA && autoResolveMSAA)
    {
        resolveMSAA(cmdList);
    }
    else
    {
        transitionToSRV(cmdList);
    }
}

void RenderTexture::transitionToRTV(ID3D12GraphicsCommandList* cmdList)
{
    if (currentState != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            texture.Get(),
            currentState,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        cmdList->ResourceBarrier(1, &barrier);
        currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
}

void RenderTexture::transitionToSRV(ID3D12GraphicsCommandList* cmdList)
{
    if (currentState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            texture.Get(),
            currentState,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        cmdList->ResourceBarrier(1, &barrier);
        currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
}

void RenderTexture::resolveMSAA(ID3D12GraphicsCommandList* cmdList)
{
    _ASSERTE(enableMSAA && autoResolveMSAA && resolvedTexture);

    CD3DX12_RESOURCE_BARRIER preResolve[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            texture.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_RESOLVE_SOURCE
        ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            resolvedTexture.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RESOLVE_DEST
        )
    };
    cmdList->ResourceBarrier(_countof(preResolve), preResolve);

    cmdList->ResolveSubresource(
        resolvedTexture.Get(), 0,
        texture.Get(), 0,
        format
    );

    CD3DX12_RESOURCE_BARRIER postResolve[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            texture.Get(),
            D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            resolvedTexture.Get(),
            D3D12_RESOURCE_STATE_RESOLVE_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        )
    };
    cmdList->ResourceBarrier(_countof(postResolve), postResolve);

    currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void RenderTexture::setRenderTarget(ID3D12GraphicsCommandList* cmdList)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvDesc.getCPUHandle();

    if (depthTexture)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvDesc.getCPUHandle();
        cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

        cmdList->ClearRenderTargetView(rtv, &clearColor.x, 0, nullptr);
        cmdList->ClearDepthStencilView(dsv,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            clearDepth, 0, 0, nullptr);
    }
    else
    {
        cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        cmdList->ClearRenderTargetView(rtv, &clearColor.x, 0, nullptr);
    }

    D3D12_VIEWPORT viewport = {
        0.0f, 0.0f,
        static_cast<float>(width),
        static_cast<float>(height),
        0.0f, 1.0f
    };

    D3D12_RECT scissor = {
        0, 0,
        static_cast<LONG>(width),
        static_cast<LONG>(height)
    };

    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissor);
}

void RenderTexture::bindAsShaderResource(ID3D12GraphicsCommandList* cmdList, UINT rootParameterIndex)
{
    cmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, srvDesc.getGPUHandle());
}

void RenderTexture::transitionToState(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES newState)
{
    if (currentState != newState)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            texture.Get(),
            currentState,
            newState
        );
        cmdList->ResourceBarrier(1, &barrier);
        currentState = newState;
    }
}