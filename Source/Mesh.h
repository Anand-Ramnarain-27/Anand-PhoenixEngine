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

        uint32_t joints[4] = { 0, 0, 0, 0 };
        float    weights[4] = { 1.f, 0.f, 0.f, 0.f };
    };

    static const D3D12_INPUT_ELEMENT_DESC InputLayout[6];
    static const UINT InputLayoutCount = 6;

    Mesh() = default;
    ~Mesh() = default;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void setData(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, int materialIndex);
    void setData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, int materialIndex);
    void uploadToGPU(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer);
    void draw(ID3D12GraphicsCommandList* cmdList) const;

    uint32_t getVertexCount() const { return (uint32_t)m_vertices.size(); }
    uint32_t getIndexCount() const { return (uint32_t)m_indices.size(); }
    int getMaterialIndex() const { return m_materialIndex; }

    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }
    const D3D12_INDEX_BUFFER_VIEW& getIndexBufferView() const { return m_indexBufferView; }

    const Vector3& getAABBMin() const { return m_aabbMin; }
    const Vector3& getAABBMax() const { return m_aabbMax; }
    bool hasAABB() const { return m_hasAABB; }
    bool isOnGPU() const { return m_hasVertexBuffer; }

    bool     isSkinned()    const { return m_isSkinned; }
    void     setIsSkinned(bool s) { m_isSkinned = s; }
    uint32_t getJointCount() const { return m_jointCount; }
    void     setJointCount(uint32_t n) { m_jointCount = n; }

    void     setMorphTargetCount(uint32_t n) { m_morphTargetCount = n; }
    uint32_t getMorphTargetCount() const { return m_morphTargetCount; }
    void     addMorphTarget(const std::vector<Vertex>& deltas);
    void     uploadMorphTargets(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* sb);

    D3D12_GPU_VIRTUAL_ADDRESS getMorphBufferVA() const {
        return m_morphVBV.BufferLocation;
    }

    bool hasMorphTargets() const { return m_morphTargetCount > 0; }

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

    bool     m_isSkinned = false;
    uint32_t m_jointCount = 0;

    uint32_t                 m_morphTargetCount = 0;
    std::vector<Vertex>      m_morphDeltas;     
    D3D12_VERTEX_BUFFER_VIEW m_morphVBV = {};
};