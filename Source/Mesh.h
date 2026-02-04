#pragma once
#include "Globals.h"
#include <vector>
#include <wrl/client.h>

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
    };

public:
    Mesh() = default;
    ~Mesh() = default;

    void load(const tinygltf::Primitive& prim, const tinygltf::Model& model);
    void render(ID3D12GraphicsCommandList* cmdList) const;

    int getMaterialIndex() const { return materialID; }

    uint32_t getVertexCount() const { return vertexCount; }
    uint32_t getIndexCount() const { return indexCount; }

    static const D3D12_INPUT_ELEMENT_DESC inputLayout[3];
private:
    bool copyVertexData(uint8_t* dest, size_t elemSize, size_t stride, size_t count,
        const tinygltf::Model& model, int accessorIndex);

    void setupGPUBuffers();

private:
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    int materialID = -1;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vbView;

    ComPtr<ID3D12Resource> indexBuffer;
    D3D12_INDEX_BUFFER_VIEW ibView;
};
