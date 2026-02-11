#pragma once

#include "Globals.h"
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class StaticMesh
{
public:
    struct Vertex
    {
        Vector3 position;
        Vector2 texCoord;
        Vector3 normal;
    };

    static const D3D12_INPUT_ELEMENT_DESC InputLayout[3];
    static const UINT InputLayoutCount = 3;

public:
    StaticMesh() = default;
    ~StaticMesh() { cleanup(); }

    StaticMesh(const StaticMesh&) = delete;
    StaticMesh& operator=(const StaticMesh&) = delete;

    void draw(ID3D12GraphicsCommandList* cmdList) const;

    void setData(const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices,
        int materialIndex);

    uint32_t getVertexCount() const { return (uint32_t)m_vertices.size(); }
    uint32_t getIndexCount()  const { return (uint32_t)m_indices.size(); }
    int getMaterialIndex() const { return m_materialIndex; }

private:
    void createBuffers();
    void cleanup();

private:
    int m_materialIndex = -1;

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};

    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
};
