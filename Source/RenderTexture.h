#pragma once

#include "ShaderTableDesc.h"
#include "RenderTargetDesc.h"
#include "DepthStencilDesc.h"

class RenderTexture
{
    struct TexturePair {
        ComPtr<ID3D12Resource> texture;
        ComPtr<ID3D12Resource> resolved;
        ComPtr<ID3D12Resource> depthTexture;
    };

    TexturePair      m_textures;
    UINT             m_width = 0;
    UINT             m_height = 0;
    DXGI_FORMAT      m_format;
    DXGI_FORMAT      m_depthFormat;
    const char* m_name;
    Vector4          m_clearColor;
    FLOAT            m_clearDepth;
    bool             m_autoResolveMSAA = false;
    bool             m_msaa = false;

    ShaderTableDesc  m_srvDesc;
    RenderTargetDesc m_rtvDesc;
    DepthStencilDesc m_dsvDesc;

    mutable D3D12_CPU_DESCRIPTOR_HANDLE m_cachedRTV = {};
    mutable D3D12_CPU_DESCRIPTOR_HANDLE m_cachedDSV = {};
    mutable D3D12_GPU_DESCRIPTOR_HANDLE m_cachedSRV = {};
    mutable bool                        m_handlesValid = false;

public:
    RenderTexture(const char* name, DXGI_FORMAT format, const Vector4& clearColor,
        DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN,
        float clearDepth = 1.0f, bool msaa = false, bool autoResolve = false)
        : m_format(format), m_depthFormat(depthFormat), m_name(name)
        , m_clearColor(clearColor), m_clearDepth(clearDepth)
        , m_msaa(msaa), m_autoResolveMSAA(autoResolve)
    {
    }

    ~RenderTexture() = default;

    bool isValid() const { return m_width > 0 && m_height > 0 && m_srvDesc.isValid(); }
    bool isMSAA()  const { return m_msaa; }

    void resize(UINT width, UINT height);
    void beginRender(ID3D12GraphicsCommandList* cmdList, bool clear = true);
    void endRender(ID3D12GraphicsCommandList* cmdList);
    void bindAsShaderResource(ID3D12GraphicsCommandList* cmdList, int slot);

    UINT getWidth()  const { return m_width; }
    UINT getHeight() const { return m_height; }

    D3D12_GPU_DESCRIPTOR_HANDLE getSrvHandle() const { ensureHandles(); return m_cachedSRV; }
    D3D12_CPU_DESCRIPTOR_HANDLE getRtvHandle() const { ensureHandles(); return m_cachedRTV; }
    D3D12_CPU_DESCRIPTOR_HANDLE getDsvHandle() const { ensureHandles(); return m_cachedDSV; }

    ID3D12Resource* getTexture()      const { return m_textures.texture.Get(); }
    ID3D12Resource* getDepthTexture() const { return m_textures.depthTexture.Get(); }

    const ShaderTableDesc& getSrvTableDesc() const { return m_srvDesc; }
    const RenderTargetDesc& getRtvDesc()      const { return m_rtvDesc; }
    const DepthStencilDesc& getDsvDesc()      const { return m_dsvDesc; }

private:
    void releaseResources();
    void ensureHandles() const { if (!m_handlesValid) updateCachedHandles(); }
    void updateCachedHandles() const;
    void resolveMSAA(ID3D12GraphicsCommandList* cmdList);
};