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
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>

class Material;

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
    bool createFallbackTable();

    // Returns a GPU descriptor handle for the material's 5 textures. The table is
    // built once per unique material and reused every frame; SRVs are only
    // recreated when the material's bound textures actually change. This replaces
    // the old per-draw, per-frame SRV churn that scaled with submesh count.
    D3D12_GPU_DESCRIPTOR_HANDLE getMaterialTableHandle(const Material* mat);

    void writePerDrawCBs(const MeshEntry& entry, const Matrix& viewProj, UINT slot,
                         int viewportIndex,
                         D3D12_GPU_VIRTUAL_ADDRESS& outMvpVA,
                         D3D12_GPU_VIRTUAL_ADDRESS& outInstVA);

    GBuffer m_gbuffer[NUM_VIEWPORTS];
    int m_activeIndex = 0;
    GBufferPipeline m_pipeline;

    // Per-frame draw cap for the geometry pass. Each slot consumes one MVP CB,
    // one instance CB, and one descriptor table from ModuleShaderDescriptors
    // (MAX_INSTANCES * NUM_VIEWPORTS tables total — keep ModuleShaderDescriptors::
    // MAX_TABLES comfortably above that). The editor/scene view draws every mesh
    // unculled, so this needs headroom for large environment scenes.
    static constexpr UINT MAX_INSTANCES = 4096;

    ComPtr<ID3D12Resource> m_mvpRing[NUM_VIEWPORTS];
    void* m_mvpMapped[NUM_VIEWPORTS] = {};

    ComPtr<ID3D12Resource> m_instanceRing[NUM_VIEWPORTS];
    void* m_instanceMapped[NUM_VIEWPORTS] = {};

    ComPtr<ID3D12Resource> m_fallbackTex;

    // One descriptor table shared by every material that has no textures bound.
    ShaderTableDesc m_fallbackTable;

    // Per-material descriptor-table cache. Keyed by the resolved Material*; the
    // stored resource pointers let us detect when a material's textures change
    // (e.g. hot reload) and rewrite only that table.
    struct MatCacheEntry {
        ShaderTableDesc table;
        ID3D12Resource* srcs[5] = {};
    };
    std::unordered_map<const Material*, MatCacheEntry> m_matTableCache;
    static constexpr size_t kMatCacheCap = 4096;
};

