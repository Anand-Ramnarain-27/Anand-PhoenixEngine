#include "Globals.h"
#include "RenderTexture.h"

#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"
#include "ShaderTableDesc.h"

RenderTexture::RenderTexture(
    DXGI_FORMAT colorFormat,
    DXGI_FORMAT depthFormat,
    const Vector4& clearColor,
    float clearDepth
)
    : colorFormat(colorFormat)
    , depthFormat(depthFormat)
    , clearColor(clearColor)
    , clearDepth(clearDepth)
{
}

RenderTexture::~RenderTexture()
{
    releaseResources();
}

void RenderTexture::resize(UINT newWidth, UINT newHeight)
{
    if (width == newWidth && height == newHeight)
        return;

    width = newWidth;
    height = newHeight;

    app->getD3D12()->flush();

    releaseResources();
    createResources();
}

void RenderTexture::createResources()
{
    ModuleResources* resources = app->getResources();
    ModuleD3D12* d3d = app->getD3D12();
    ModuleShaderDescriptors* shaderDescriptors = app->getShaderDescriptors();

    colorTexture = resources->createRenderTarget(
        colorFormat,
        width,
        height,
        1,
        clearColor,
        "RenderTexture"
    );

    rtvHandle = d3d->createRTV(colorTexture.Get());


    auto table = shaderDescriptors->allocTable("RenderTexture");
    table->createTexture2DSRV(0, colorTexture.Get());
    srvGpuHandle = table->getGPUHandle();       


    if (depthFormat != DXGI_FORMAT_UNKNOWN)
    {
        depthTexture = resources->createDepthStencil(
            depthFormat,
            width,
            height,
            1,
            clearDepth,
            0,
            "RenderTextureDepth"
        );

        dsvHandle = d3d->createDSV(depthTexture.Get());
    }
}


void RenderTexture::releaseResources()
{
    colorTexture.Reset();
    depthTexture.Reset();
}

void RenderTexture::beginRender(ID3D12GraphicsCommandList* cmdList)
{
    CD3DX12_RESOURCE_BARRIER toRT =
        CD3DX12_RESOURCE_BARRIER::Transition(
            colorTexture.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );

    cmdList->ResourceBarrier(1, &toRT);

    if (depthTexture)
    {
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
        cmdList->ClearDepthStencilView(
            dsvHandle,
            D3D12_CLEAR_FLAG_DEPTH,
            clearDepth,
            0,
            0,
            nullptr
        );
    }
    else
    {
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    }

    cmdList->ClearRenderTargetView(
        rtvHandle,
        reinterpret_cast<const float*>(&clearColor),
        0,
        nullptr
    );

    D3D12_VIEWPORT vp{ 0, 0, float(width), float(height), 0.0f, 1.0f };
    D3D12_RECT scissor{ 0, 0, LONG(width), LONG(height) };

    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);
}

void RenderTexture::endRender(ID3D12GraphicsCommandList* cmdList)
{
    CD3DX12_RESOURCE_BARRIER toSRV =
        CD3DX12_RESOURCE_BARRIER::Transition(
            colorTexture.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );

    cmdList->ResourceBarrier(1, &toSRV);
}
