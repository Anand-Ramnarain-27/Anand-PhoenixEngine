#pragma once
#include "GBuffer.h"
#include "MeshRenderPass.h" 
#include "ShaderTableDesc.h"
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class EnvironmentSystem;

class DeferredLightingPass {
public:
    bool init(ID3D12Device* device);

    // Reads G-Buffer, applies lighting, writes to currently bound RTV.
    // Call AFTER GBufferPass::render() has transitioned GBuffer back to SRV.
    void render(ID3D12GraphicsCommandList* cmd,
        GBuffer& gbuffer,
        ID3D12Resource* depthSRVResource,
        const FrameLightData& lights,
        const Vector3& cameraPos,
        const Matrix& view,
        const Matrix& proj,
        const EnvironmentSystem* env);

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);
    bool createLightBuffers(ID3D12Device* device);
    bool createDepthSRVTable();
    bool createFallbackIBL(ID3D12Device* device);

    struct alignas(256) LightCB {
        Matrix   invViewProj;
        Vector3  cameraPosition;
        uint32_t dirLightCount;
        uint32_t pointLightCount;
        uint32_t spotLightCount;
        uint32_t envRoughnessLevels;
        float    pad;
    };

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;

    ComPtr<ID3D12Resource> m_lightCB;
    void* m_lightMapped = nullptr;
    ComPtr<ID3D12Resource> m_dirBuf, m_pointBuf, m_spotBuf;
    void* m_dirMapped, * m_pointMapped, * m_spotMapped;
    ShaderTableDesc        m_lightSRVTable;

    ShaderTableDesc        m_depthSRVTable;

    ComPtr<ID3D12Resource> m_fallbackCube;
    ShaderTableDesc        m_fallbackIBLTable;
};
