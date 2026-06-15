#pragma once

#include "GBuffer.h"
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class GBufferPipeline {
public:
    static constexpr UINT SLOT_MVP_CB = 0;
    static constexpr UINT SLOT_INSTANCE_CB = 1;
    static constexpr UINT SLOT_MAT_TEXTURES = 2;
    static constexpr UINT SLOT_SAMPLER = 3;

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO() const { return m_pso.Get(); }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};
#include "MeshEntry.h"
#include "MeshPipeline.h"
#include "ShaderTableDesc.h"
#include <vector>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class GBufferPass {
public:
    GBufferPass() = default;
    ~GBufferPass() = default;

    bool init(ID3D12Device* device);

    void render(ID3D12GraphicsCommandList* cmd,
                const std::vector<MeshEntry*>& meshes,
                const Matrix& viewProj,
                uint32_t width, uint32_t height,
                int viewportIndex);

    GBuffer& getGBuffer(){ return m_gbuffer[m_activeIndex]; }
    GBufferPipeline& getPipeline(){ return m_pipeline; }

    static constexpr int NUM_VIEWPORTS = 2;

private:
    bool createUploadBuffers(ID3D12Device* device);
    bool createFallbackTexture(ID3D12Device* device);
    bool createMatTableRing();

    void writePerDrawCBs(const MeshEntry& entry, const Matrix& viewProj, UINT slot,
                         int viewportIndex,
                         D3D12_GPU_VIRTUAL_ADDRESS& outMvpVA,
                         D3D12_GPU_VIRTUAL_ADDRESS& outInstVA);

    GBuffer m_gbuffer[NUM_VIEWPORTS];
    int m_activeIndex = 0;
    GBufferPipeline m_pipeline;

    static constexpr UINT MAX_INSTANCES = 512;

    ComPtr<ID3D12Resource> m_mvpRing[NUM_VIEWPORTS];
    void* m_mvpMapped[NUM_VIEWPORTS] = {};

    ComPtr<ID3D12Resource> m_instanceRing[NUM_VIEWPORTS];
    void* m_instanceMapped[NUM_VIEWPORTS] = {};

    ComPtr<ID3D12Resource> m_fallbackTex;
    std::vector<ShaderTableDesc> m_matRing[NUM_VIEWPORTS];
};

