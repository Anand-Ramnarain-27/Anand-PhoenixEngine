#pragma once

#include "Globals.h"
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class ModuleStaticBuffer;

class Mesh {
public:
    struct Vertex {
        Vector3 position;
        Vector2 texCoord;
        Vector3 normal;
        Vector4 tangent;
    };

    struct BoneWeight {
        int   indices[4] = {0, 0, 0, 0};
        float weights[4] = {0.f, 0.f, 0.f, 0.f};
    };

    // Per-vertex delta stored for each morph target.
    // Three float3 fields padded to float4 each to match HLSL StructuredBuffer layout.
    struct MorphVertex {
        Vector3 deltaPosition; float _pad0 = 0.f;
        Vector3 deltaNormal;   float _pad1 = 0.f;
        Vector3 deltaTangent;  float _pad2 = 0.f;
    };

    struct MorphTarget {
        std::string name;
        float       defaultWeight = 0.f;
    };

    static const D3D12_INPUT_ELEMENT_DESC InputLayout[4];
    static const UINT InputLayoutCount = 4;

    // Bone weight buffer layout: bound to input slot 1
    static const D3D12_INPUT_ELEMENT_DESC BoneWeightInputLayout[2];
    static const UINT BoneWeightInputLayoutCount = 2;

    Mesh() = default;
    ~Mesh() = default;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void setData(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, int materialIndex);
    void setData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, int materialIndex);
    void setBoneWeights(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer, const std::vector<BoneWeight>& boneWeights);
    void setMorphTargets(const std::vector<MorphTarget>& targets, const std::vector<MorphVertex>& vertexData);
    void uploadToGPU(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer);
    void draw(ID3D12GraphicsCommandList* cmdList) const;
    // Draw using a pre-skinned vertex buffer produced by SkinningPass instead of the
    // original vertex buffer.  skinnedVA is the GPU VA of the first vertex for this mesh
    // within SkinningPass::getOutputBuffer() (i.e. already offset by vertexOffset * sizeof(Vertex)).
    void drawSkinned(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_VIRTUAL_ADDRESS skinnedVA) const;

    uint32_t getVertexCount() const { return (uint32_t)m_vertices.size(); }
    uint32_t getIndexCount() const { return (uint32_t)m_indices.size(); }
    int getMaterialIndex() const { return m_materialIndex; }

    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }
    const std::vector<BoneWeight>& getBoneWeights() const { return m_boneWeights; }
    bool hasBoneWeights() const { return m_hasBoneWeightBuffer || !m_boneWeights.empty(); }
    const std::vector<MorphTarget>& getMorphTargets() const { return m_morphTargets; }
    uint32_t getNumMorphTargets() const { return m_numMorphTargets; }
    bool hasMorphTargets() const { return m_numMorphTargets > 0; }

    // GPU virtual addresses — valid after uploadToGPU() or setData() with a command list.
    D3D12_GPU_VIRTUAL_ADDRESS getVertexBufferVA()       const { return m_vertexBufferView.BufferLocation; }
    D3D12_GPU_VIRTUAL_ADDRESS getBoneWeightBufferVA()   const { return m_boneWeightBufferView.BufferLocation; }
    D3D12_GPU_VIRTUAL_ADDRESS getMorphTargetBufferVA()  const;

    const Vector3& getAABBMin() const { return m_aabbMin; }
    const Vector3& getAABBMax() const { return m_aabbMax; }
    bool hasAABB() const { return m_hasAABB; }
    bool isOnGPU() const { return m_hasVertexBuffer && (m_boneWeights.empty() || m_hasBoneWeightBuffer) && (m_morphVertexData.empty() || m_hasMorphTargetBuffer); }

private:
    void computeAABB();
    void createLegacyBuffers();

    int m_materialIndex = -1;
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
    bool m_hasVertexBuffer = false;
    bool m_hasIndexBuffer = false;

    std::vector<BoneWeight>  m_boneWeights;
    D3D12_VERTEX_BUFFER_VIEW m_boneWeightBufferView = {};
    bool                     m_hasBoneWeightBuffer = false;

    std::vector<MorphTarget>  m_morphTargets;
    std::vector<MorphVertex>  m_morphVertexData;
    ComPtr<ID3D12Resource>    m_morphTargetBuffer;
    uint32_t                  m_numMorphTargets = 0;
    bool                      m_hasMorphTargetBuffer = false;

    ComPtr<ID3D12Resource> m_legacyVertexBuffer;
    ComPtr<ID3D12Resource> m_legacyIndexBuffer;

    Vector3 m_aabbMin = {};
    Vector3 m_aabbMax = {};
    bool m_hasAABB = false;
};