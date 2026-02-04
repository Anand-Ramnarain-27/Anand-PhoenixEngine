// RenderTexture.h
#pragma once

#include "ShaderTableDesc.h"
#include "RenderTargetDesc.h"
#include "DepthStencilDesc.h"

// Enhanced RenderTexture with better resource management and efficiency
class RenderTexture
{
    struct TexturePair {
        ComPtr<ID3D12Resource> texture;
        ComPtr<ID3D12Resource> resolved;
        ComPtr<ID3D12Resource> depthTexture;
    };

    TexturePair m_textures;
    UINT m_width = 0;
    UINT m_height = 0;

    DXGI_FORMAT m_format;
    DXGI_FORMAT m_depthFormat;
    const char* m_name;
    Vector4 m_clearColor;
    FLOAT m_clearDepth;
    bool m_autoResolveMSAA = false;
    bool m_msaa = false;

    std::shared_ptr<ShaderTableDesc> m_srvDesc;
    RenderTargetDesc m_rtvDesc;
    DepthStencilDesc m_dsvDesc;

    // Cached handles for performance
    mutable D3D12_CPU_DESCRIPTOR_HANDLE m_cachedRTV = {};
    mutable D3D12_CPU_DESCRIPTOR_HANDLE m_cachedDSV = {};
    mutable D3D12_GPU_DESCRIPTOR_HANDLE m_cachedSRV = {};
    mutable bool m_handlesValid = false;

public:
    RenderTexture(const char* name, DXGI_FORMAT format, const Vector4& clearColor,
        DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN,
        float clearDepth = 1.0f, bool msaa = false, bool autoResolve = false)
        : m_format(format), m_depthFormat(depthFormat), m_name(name),
        m_clearColor(clearColor), m_clearDepth(clearDepth),
        m_msaa(msaa), m_autoResolveMSAA(autoResolve) {
    };

    ~RenderTexture();

    bool isValid() const { return m_width > 0 && m_height > 0; }
    bool isMSAA() const { return m_msaa; }

    void resize(UINT width, UINT height);

    // Single call to set up rendering
    void beginRender(ID3D12GraphicsCommandList* cmdList, bool clear = true);

    // Single call to finish rendering
    void endRender(ID3D12GraphicsCommandList* cmdList);

    // Quick bind helper
    void bindAsShaderResource(ID3D12GraphicsCommandList* cmdList, int slot);

    // Getters with cached performance
    UINT getWidth() const { return m_width; }
    UINT getHeight() const { return m_height; }

    D3D12_GPU_DESCRIPTOR_HANDLE getSrvHandle() const {
        if (!m_handlesValid) updateCachedHandles();
        return m_cachedSRV;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE getRtvHandle() const {
        if (!m_handlesValid) updateCachedHandles();
        return m_cachedRTV;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE getDsvHandle() const {
        if (!m_handlesValid) updateCachedHandles();
        return m_cachedDSV;
    }

    ID3D12Resource* getTexture() const { return m_textures.texture.Get(); }
    ID3D12Resource* getDepthTexture() const { return m_textures.depthTexture.Get(); }

private:
    void createResources();
    void releaseResources();
    void updateCachedHandles() const;

    void resolveMSAA(ID3D12GraphicsCommandList* cmdList);
    void transition(ID3D12GraphicsCommandList* cmdList,
        D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after,
        ID3D12Resource* resource = nullptr);
};