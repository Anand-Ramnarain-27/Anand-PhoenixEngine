#pragma once

#include "Globals.h"

class RenderTexture
{
public:
    RenderTexture(
        DXGI_FORMAT colorFormat,
        DXGI_FORMAT depthFormat,
        const Vector4& clearColor,
        float clearDepth = 1.0f
    );

    ~RenderTexture();

    void resize(UINT width, UINT height);

    void beginRender(ID3D12GraphicsCommandList* cmdList);
    void endRender(ID3D12GraphicsCommandList* cmdList);

    D3D12_GPU_DESCRIPTOR_HANDLE getSrvHandle() const { return srvGpuHandle; }

    UINT getWidth() const { return width; }
    UINT getHeight() const { return height; }

private:
    void createResources();
    void releaseResources();

private:
    ComPtr<ID3D12Resource> colorTexture;
    ComPtr<ID3D12Resource> depthTexture;

    DXGI_FORMAT colorFormat;
    DXGI_FORMAT depthFormat;

    Vector4 clearColor;
    float clearDepth;

    UINT width = 0;
    UINT height = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle{};
};
