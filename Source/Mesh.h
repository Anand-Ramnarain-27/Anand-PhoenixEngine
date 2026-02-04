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
        Vector2 texCoord;
        Vector3 normal;
    };

public:
    Mesh();
    ~Mesh();

    void load(const tinygltf::Primitive& primitive, const tinygltf::Model& model);

    void draw(ID3D12GraphicsCommandList* commandList) const;

    uint32_t getVertexCount() const { return m_vertexCount; }
    uint32_t getIndexCount() const { return m_indexCount; }
    int getMaterialIndex() const { return m_materialIndex; }

    static const D3D12_INPUT_ELEMENT_DESC* getInputLayout() { return s_inputLayout; }
    static uint32_t getInputLayoutCount() { return 3; }

private:
    void createBuffers();

    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;
    int m_materialIndex = -1;

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    static const D3D12_INPUT_ELEMENT_DESC s_inputLayout[3];
};