#pragma once
// Geometry pass: draws opaque meshes into the G-buffer.

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
    static constexpr UINT SLOT_OCCLUSION = 4;   // b2, root constants (OcclusionParams)

    bool init(ID3D12Device* device);

    ID3D12PipelineState* getPSO(bool doubleSided = false) const {
        return doubleSided ? m_psoDoubleSided.Get() : m_pso.Get();
    }
    ID3D12RootSignature* getRootSig() const { return m_rootSig.Get(); }

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
    ComPtr<ID3D12PipelineState> m_psoDoubleSided;
};
#include "MeshEntry.h"
#include "MeshPipeline.h"
#include "ShaderTableDesc.h"
#include <vector>
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>

class Material;

// Occlusion fade (wall cut-out) parameters, set once per GBuffer pass as root constants (b2).
// Layout must match cbuffer OcclusionCB in GBufferPass.hlsli.
struct OcclusionParams {
    Vector3 cameraPos = Vector3::Zero;
    float radius = 1.6f;
    Vector3 focusPos = Vector3::Zero;
    float feather = 0.5f;
    float focusFeetY = 0.0f;
    float floorClearance = 0.25f;
    float coneNearScale = 0.35f;
    float enabled = 0.0f;
};
static_assert(sizeof(OcclusionParams) == 12 * sizeof(float), "OcclusionParams must stay 12 DWORDs");

using Microsoft::WRL::ComPtr;

/// Owns the G-buffer per viewport and records the opaque draws, including the occlusion fade cut-out.
class GBufferPass {
public:
    GBufferPass() = default;
    ~GBufferPass() = default;

    bool init(ID3D12Device* device);

    void render(ID3D12GraphicsCommandList* cmd,
                const std::vector<MeshEntry*>& meshes,
                const Matrix& viewProj,
                uint32_t width, uint32_t height,
                int viewportIndex,
                const OcclusionParams& occlusion = OcclusionParams());

    GBuffer& getGBuffer(){ return m_gbuffer[m_activeIndex]; }
    GBufferPipeline& getPipeline(){ return m_pipeline; }

    static constexpr int NUM_VIEWPORTS = 2;
    // Buffers the CPU writes every frame exist once per (frame in flight, viewport): the CPU records up to
    // FRAMES_IN_FLIGHT-1 frames ahead of the GPU, so with a single copy per viewport the next frame's
    // writes land in data the GPU is still reading (seen as Game view frames drawn with the wrong
    // matrices or lighting). GPU-written resources stay one per viewport - commands run in order.
    static constexpr int NUM_UPLOAD_SLOTS = NUM_VIEWPORTS * FRAMES_IN_FLIGHT;

private:
    // Index into the NUM_UPLOAD_SLOTS arrays for this frame's copy of `viewportIndex`'s buffers.
    static int uploadSlot(int viewportIndex);
    bool createUploadBuffers(ID3D12Device* device);
    bool createFallbackTexture(ID3D12Device* device);
    bool createFallbackTable();

    D3D12_GPU_DESCRIPTOR_HANDLE getMaterialTableHandle(const Material* mat);

    void writePerDrawCBs(const MeshEntry& entry, const Matrix& viewProj, UINT slot,
                         int viewportIndex,
                         D3D12_GPU_VIRTUAL_ADDRESS& outMvpVA,
                         D3D12_GPU_VIRTUAL_ADDRESS& outInstVA);

    GBuffer m_gbuffer[NUM_VIEWPORTS];
    int m_activeIndex = 0;
    GBufferPipeline m_pipeline;

    // Per-frame draw cap for the geometry pass (one MVP CB + one instance CB per slot).
    static constexpr UINT MAX_INSTANCES = 4096;

    ComPtr<ID3D12Resource> m_mvpRing[NUM_UPLOAD_SLOTS];
    void* m_mvpMapped[NUM_UPLOAD_SLOTS] = {};

    ComPtr<ID3D12Resource> m_instanceRing[NUM_UPLOAD_SLOTS];
    void* m_instanceMapped[NUM_UPLOAD_SLOTS] = {};

    ComPtr<ID3D12Resource> m_fallbackTex;
    ShaderTableDesc m_fallbackTable;

    struct MatCacheEntry {
        ShaderTableDesc table;
        ID3D12Resource* srcs[5] = {};
    };
    std::unordered_map<const Material*, MatCacheEntry> m_matTableCache;
    static constexpr size_t kMatCacheCap = 4096;
};

