#pragma once

#include "Globals.h"
#include "ShaderTableDesc.h"
#include "RenderTargetDesc.h"
#include "DepthStencilDesc.h"

class RenderTexture
{
    ComPtr<ID3D12Resource> texture;
    ComPtr<ID3D12Resource> resolvedTexture;
    ComPtr<ID3D12Resource> depthTexture;

    ShaderTableDesc srvDesc;
    RenderTargetDesc rtvDesc;
    DepthStencilDesc dsvDesc;

    UINT width = 0;
    UINT height = 0;
    DXGI_FORMAT format;
    DXGI_FORMAT depthFormat;
    const char* name;
    Vector4 clearColor;
    float clearDepth;
    bool enableMSAA = false;
    bool autoResolveMSAA = false;
    UINT msaaSampleCount = 1;

    D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;

public:
    RenderTexture(const char* name,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
        const Vector4& clearColor = Vector4(0, 0, 0, 1),
        DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN,
        float clearDepth = 1.0f,
        bool msaa = false,
        UINT sampleCount = 4,
        bool autoResolve = false)
        : format(format)
        , depthFormat(depthFormat)
        , name(name)
        , clearColor(clearColor)
        , clearDepth(clearDepth)
        , enableMSAA(msaa)
        , msaaSampleCount(msaa ? sampleCount : 1)
        , autoResolveMSAA(autoResolve&& msaa)
    {
    }

    ~RenderTexture();

    void resize(UINT newWidth, UINT newHeight);
    void beginRendering(ID3D12GraphicsCommandList* cmdList);
    void endRendering(ID3D12GraphicsCommandList* cmdList);

    bool isValid() const { return width > 0 && height > 0; }
    UINT getWidth() const { return width; }
    UINT getHeight() const { return height; }
    ID3D12Resource* getTexture() const { return texture.Get(); }
    ID3D12Resource* getResolvedTexture() const { return resolvedTexture.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE getSRVHandle() const { return srvDesc.getGPUHandle(); }
    const ShaderTableDesc& getSRVDesc() const { return srvDesc; }
    D3D12_CPU_DESCRIPTOR_HANDLE getRTVHandle() const { return rtvDesc.getCPUHandle(); }
    D3D12_CPU_DESCRIPTOR_HANDLE getDSVHandle() const { return dsvDesc.getCPUHandle(); }

    void bindAsShaderResource(ID3D12GraphicsCommandList* cmdList, UINT rootParameterIndex = 0);
    void transitionToState(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES newState);

private:
    void releaseResources();
    void createResources();
    void updateDescriptors();

    void transitionToRTV(ID3D12GraphicsCommandList* cmdList);
    void transitionToSRV(ID3D12GraphicsCommandList* cmdList);
    void resolveMSAA(ID3D12GraphicsCommandList* cmdList);
    void setRenderTarget(ID3D12GraphicsCommandList* cmdList);

    ID3D12Resource* getSRVTexture() const {
        return (enableMSAA && autoResolveMSAA) ? resolvedTexture.Get() : texture.Get();
    }
};