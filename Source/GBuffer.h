#pragma once

#include "ShaderTableDesc.h"
#include "RenderTargetDesc.h"
#include "DepthStencilDesc.h"

class GBuffer {
public:
    static constexpr int NUM_COLOR_RTS = 3;

    enum Target : int {
        Albedo          = 0,
        NormalMetalRough = 1,
        EmissiveAO       = 2
    };

    static constexpr DXGI_FORMAT kAlbedoFormat          = DXGI_FORMAT_R8G8B8A8_UNORM;
    static constexpr DXGI_FORMAT kNormalMetalRoughFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT kEmissiveAOFormat       = DXGI_FORMAT_R8G8B8A8_UNORM;
    static constexpr DXGI_FORMAT kDepthFormat            = DXGI_FORMAT_D32_FLOAT;

    bool     isValid()    const { return m_width > 0 && m_height > 0; }
    uint32_t getWidth()   const { return m_width; }
    uint32_t getHeight()  const { return m_height; }

    void resize(uint32_t w, uint32_t h);
    void release();

    // Transition all color RTs from SRV to RTV, bind them + DSV, clear, set viewport/scissor
    void beginGeomPass(ID3D12GraphicsCommandList* cmd, bool clear = true);
    // Transition all color RTs from RTV back to SRV
    void endGeomPass(ID3D12GraphicsCommandList* cmd);

    D3D12_GPU_DESCRIPTOR_HANDLE getSrvHandle(Target t)   const;
    D3D12_GPU_DESCRIPTOR_HANDLE getDepthSrvHandle()      const;
    D3D12_CPU_DESCRIPTOR_HANDLE getDsvHandle()           const;
    ID3D12Resource*             getDepthTexture()        const { return m_depthTexture.Get(); }

private:
    ComPtr<ID3D12Resource> m_colorTextures[NUM_COLOR_RTS];
    ComPtr<ID3D12Resource> m_depthTexture;

    RenderTargetDesc m_rtvDescs[NUM_COLOR_RTS];
    DepthStencilDesc m_dsvDesc;
    ShaderTableDesc  m_srvTables[NUM_COLOR_RTS];
    ShaderTableDesc  m_depthSrvTable;

    bool     m_depthReadable = false;
    uint32_t m_width  = 0;
    uint32_t m_height = 0;
};
