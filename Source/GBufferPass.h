#pragma once
#include "MeshPipeline.h"
#include "MeshEntry.h"
#include "GBuffer.h"
#include "DepthStencilDesc.h"
#include "ShaderTableDesc.h"
#include <vector>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class GBufferPass {
public:
    bool init(ID3D12Device* device);

    // Renders opaque meshes into the G-Buffer.
    // Depth comes from the viewport's existing depth texture.
    void render(ID3D12GraphicsCommandList* cmd,
        const std::vector<MeshEntry*>& meshes,
        const Vector3& cameraPos,
        const Matrix& viewProj,
        GBuffer& gbuffer,
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle,
        uint32_t width, uint32_t height);

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);
    bool createUploadBuffers(ID3D12Device* device);
    bool createFallbackTextures(ID3D12Device* device);
    bool createMatTableRing();

    void bindMaterialTextures(ShaderTableDesc& tbl, const Material* mat);

    void writePerDrawCBs(const MeshEntry& entry, const Matrix& viewProj,
        UINT slot,
        D3D12_GPU_VIRTUAL_ADDRESS& outMvpVA,
        D3D12_GPU_VIRTUAL_ADDRESS& outInstVA);

    static constexpr UINT MAX_INSTANCES = 512;
    static constexpr UINT cbAlign(UINT b) { return (b + 255u) & ~255u; }

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;

    ComPtr<ID3D12Resource> m_mvpRing;
    void* m_mvpMapped = nullptr;
    ComPtr<ID3D12Resource> m_instanceRing;
    void* m_instanceMapped = nullptr;

    ComPtr<ID3D12Resource> m_fallbackTex2D;
    std::vector<ShaderTableDesc> m_matRing;
};
