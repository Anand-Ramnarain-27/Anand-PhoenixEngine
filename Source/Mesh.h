#pragma once

#include "Globals.h"
#include <string>
#include <vector>
#include <wrl/client.h>
#include "UID.h"

using Microsoft::WRL::ComPtr;

class MeshImporter;

class Mesh
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
    Mesh();
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&&) = default;
    Mesh& operator=(Mesh&&) = default;

    // ?? Phase 3 runtime loading
    bool loadFromBinary(const char* path);

    void draw(ID3D12GraphicsCommandList* commandList) const;

    uint32_t getVertexCount() const { return (uint32_t)m_vertices.size(); }
    uint32_t getIndexCount() const { return (uint32_t)m_indices.size(); }

    UID GetUID() const { return uid; }

private:
    friend class MeshImporter;

    void createBuffers();
    void cleanup();

private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};

    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};

    UID uid;
};
