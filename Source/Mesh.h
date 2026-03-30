#pragma once

#include "Globals.h"
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
        uint32_t indices[4] = { 0,0,0,0 };
        float    weights[4] = { 0,0,0,0 };
    };

    static const D3D12_INPUT_ELEMENT_DESC InputLayout[4];
    static const UINT InputLayoutCount = 4;

    Mesh() = default;
    ~Mesh() = default;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void setData(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, int materialIndex);
    void setData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, int materialIndex);
    void uploadToGPU(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer);
    void draw(ID3D12GraphicsCommandList* cmdList) const;
    void drawIndexOnly(ID3D12GraphicsCommandList* cmd) const;

    uint32_t getVertexCount() const { return (uint32_t)m_vertices.size(); }
    uint32_t getIndexCount() const { return (uint32_t)m_indices.size(); }
    int getMaterialIndex() const { return m_materialIndex; }

    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }

    const Vector3& getAABBMin() const { return m_aabbMin; }
    const Vector3& getAABBMax() const { return m_aabbMax; }
    bool hasAABB() const { return m_hasAABB; }
    bool isOnGPU() const { return m_hasVertexBuffer; }

    void setSkinData(const std::vector<BoneWeight>& bw, ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* sb);
    void setMorphData(const std::vector<Vertex>& allDeltas, uint32_t numTargets, ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* sb);

    bool hasSkinData() const { return m_skinWeightsGPUVA != 0; }
    D3D12_GPU_VIRTUAL_ADDRESS getSkinWeightsVA() const { return m_skinWeightsGPUVA; }
    bool hasMorphTargets() const { return m_numMorphTargets > 0; }
    uint32_t getMorphTargetCount() const { return m_numMorphTargets; }
    D3D12_GPU_VIRTUAL_ADDRESS getMorphVertsVA() const { return m_morphVertsGPUVA; }

    void storeSkinDataCPU(const std::vector<BoneWeight>& bw) { m_skinWeightsCPU = bw; }

    D3D12_GPU_VIRTUAL_ADDRESS getVertexBufferVA() const { return m_vertexBufferView.BufferLocation; }

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

    ComPtr<ID3D12Resource> m_legacyVertexBuffer;
    ComPtr<ID3D12Resource> m_legacyIndexBuffer;

    Vector3 m_aabbMin = {};
    Vector3 m_aabbMax = {};
    bool m_hasAABB = false;

    D3D12_GPU_VIRTUAL_ADDRESS m_skinWeightsGPUVA = 0;
    uint32_t m_skinWeightCount = 0;

    std::vector<BoneWeight> m_skinWeightsCPU;

    D3D12_GPU_VIRTUAL_ADDRESS m_morphVertsGPUVA = 0;
    uint32_t m_numMorphTargets = 0;
};