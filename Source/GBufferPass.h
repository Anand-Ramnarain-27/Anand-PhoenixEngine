#pragma once

#include "GBuffer.h"
#include "GBufferPipeline.h"
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
