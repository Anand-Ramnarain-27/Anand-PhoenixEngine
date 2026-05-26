#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include "ResourceModel.h"
#include "Mesh.h"

using Microsoft::WRL::ComPtr;

// GPU skinning compute pass.
// Each frame: builds the matrix palette (invBind * jointWorld per joint),
// uploads it via a persistent upload ring, copies to a per-frame GPU palette buffer,
// then dispatches the skinning CS which writes skinned vertices to an output buffer
// that transitions VERTEX_AND_CONSTANT_BUFFER → UNORDERED_ACCESS → (dispatch) →
// VERTEX_AND_CONSTANT_BUFFER.
class SkinningPass {
public:
    // Hard caps for the combined per-frame buffers.
    static constexpr uint32_t MAX_TOTAL_JOINTS   = 1024;
    static constexpr uint32_t MAX_TOTAL_VERTICES = 65536;
    static constexpr uint32_t THREAD_GROUP_SIZE  = 64;

    // One SkinJob describes one skinned-mesh instance to process this frame.
    // paletteOffset and vertexOffset are assigned by the caller when building the job list
    // (pack all jobs into the combined palette/output buffers without gaps).
    // A job may be skin-only (skin != nullptr, no morph targets), morph-only (skin == nullptr,
    // mesh->hasMorphTargets()), or both.
    struct SkinJob {
        const ResourceModel::Skin*  skin;                // inverse bind matrices + joint count; null for morph-only jobs
        std::vector<Matrix>         jointWorldMatrices;  // world matrix per joint, in joint order
        Mesh*                       mesh;                // source mesh
        uint32_t                    paletteOffset;       // first joint index in the combined palette
        uint32_t                    vertexOffset;        // first vertex index in the combined output
        D3D12_GPU_VIRTUAL_ADDRESS   morphWeightsVA = 0;  // GPU float[] of per-target weights; 0 when no morph targets
    };

    bool init(ID3D12Device* device);
    void cleanUp();

    // Builds the matrix palette, uploads to GPU, transitions the output buffer,
    // dispatches the skinning CS for every job, then transitions the output back.
    // Must be called before the render pass that consumes the skinned vertices.
    void dispatch(ID3D12GraphicsCommandList* cmd,
                  const std::vector<SkinJob>& jobs,
                  UINT frameIndex);

    // Returns the output buffer for frameIndex after dispatch().
    // Bind as a VBV: BufferLocation = getOutputBuffer(f)->GetGPUVirtualAddress()
    //                                 + job.vertexOffset * sizeof(Mesh::Vertex)
    ID3D12Resource* getOutputBuffer(UINT frameIndex) const { return m_outputs[frameIndex].Get(); }

private:
    bool createBuffers(ID3D12Device* device);
    bool createPipeline(ID3D12Device* device);

    // Upload ring: FRAMES_IN_FLIGHT sections each for palette and paletteNormal.
    // Two consecutive sections of MAX_TOTAL_JOINTS matrices, persistently CPU-mapped.
    ComPtr<ID3D12Resource> m_upload;
    uint8_t*               m_uploadMapped = nullptr;

    // Per-frame GPU palette buffers (default heap, StructuredBuffer<float4x4> SRV).
    ComPtr<ID3D12Resource> m_palettes[FRAMES_IN_FLIGHT];
    D3D12_RESOURCE_STATES  m_paletteStates[FRAMES_IN_FLIGHT] = {};

    // Per-frame GPU inverse-transpose palette buffers (default heap, StructuredBuffer<float4x4> SRV).
    ComPtr<ID3D12Resource> m_paletteNormals[FRAMES_IN_FLIGHT];
    D3D12_RESOURCE_STATES  m_paletteNormalStates[FRAMES_IN_FLIGHT] = {};

    // Per-frame skinned-vertex output buffers (default heap, UAV + VBV).
    ComPtr<ID3D12Resource> m_outputs[FRAMES_IN_FLIGHT];

    // Small zeroed buffer bound to unused root SRV slots (morph or bone-weight slots on jobs
    // that don't use them) so every slot always has a valid GPU VA.
    ComPtr<ID3D12Resource> m_dummyBuffer;

    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;
};
