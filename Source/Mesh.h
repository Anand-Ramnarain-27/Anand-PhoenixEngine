#pragma once

#include "Globals.h"
#include <string>
#include <vector>
#include <wrl/client.h>
#include "UID.h"

using Microsoft::WRL::ComPtr;

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
        Vector2 texCoord;
        Vector3 normal;

        Vertex() = default;
        Vertex(const Vector3& pos, const Vector2& tex, const Vector3& norm)
            : position(pos), texCoord(tex), normal(norm) {
        }
    };

    static const D3D12_INPUT_ELEMENT_DESC InputLayout[3];
    static const UINT InputLayoutCount = 3;

public:
    Mesh() = default;
    ~Mesh() { cleanup(); }

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&&) = default;
    Mesh& operator=(Mesh&&) = default;

    bool load(const tinygltf::Primitive& primitive, const tinygltf::Model& model);

    void draw(ID3D12GraphicsCommandList* commandList) const;

    uint32_t getVertexCount() const { return static_cast<uint32_t>(m_vertices.size()); }
    uint32_t getIndexCount() const { return static_cast<uint32_t>(m_indices.size()); }
    int getMaterialIndex() const { return m_materialIndex; }

    static const D3D12_INPUT_ELEMENT_DESC* getInputLayout() { return InputLayout; }
    static uint32_t getInputLayoutCount() { return InputLayoutCount; }

    UID GetUID() const { return uid; }
private:
    void createBuffers();
    void cleanup();
    bool loadVertices(const tinygltf::Primitive& primitive, const tinygltf::Model& model);
    bool loadIndices(const tinygltf::Primitive& primitive, const tinygltf::Model& model);

    template<typename T>
    void copyVertexData(const uint8_t* srcData, size_t srcStride, size_t count, size_t offset);

    int m_materialIndex = -1;
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};

    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};

    UID uid;
};