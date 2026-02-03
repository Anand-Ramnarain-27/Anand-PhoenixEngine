#pragma once

#include "Globals.h"

namespace tinygltf
{
    class Model;
    struct Primitive;
}

class Mesh
{
public:
    struct Vertex
    {
        Vector3 position;
        Vector3 normal;
        Vector2 texCoord;
        Vector3 tangent;

        Vertex() = default;
        Vertex(const Vector3& pos, const Vector3& norm, const Vector2& uv, const Vector3& tan = Vector3::UnitX)
            : position(pos), normal(norm), texCoord(uv), tangent(tan) {
        }
    };

public:
    Mesh() = default;
    ~Mesh() = default;

    // Enable move semantics
    Mesh(Mesh&&) = default;
    Mesh& operator=(Mesh&&) = default;

    // Disable copy
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    bool load(const tinygltf::Primitive& prim, const tinygltf::Model& model);
    void render(ID3D12GraphicsCommandList* cmdList) const;

    int getMaterialIndex() const { return m_materialID; }
    uint32_t getVertexCount() const { return m_vertexCount; }
    uint32_t getIndexCount() const { return m_indexCount; }

    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }

    void calculateTangents();

    static const D3D12_INPUT_ELEMENT_DESC InputLayout[];

private:
    bool copyAccessorData(uint8_t* dest, size_t elementSize, size_t stride,
        const tinygltf::Model& model, int accessorIndex);
    void createGPUBuffers();

private:
    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;
    int m_materialID = -1;

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vbView = {};

    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_ibView = {};
};