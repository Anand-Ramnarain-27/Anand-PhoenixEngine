#include "Globals.h"
#include "Mesh.h"
#include "Application.h"
#include "ModuleResources.h"
#include "tiny_gltf.h"
#include <cstring>

const D3D12_INPUT_ELEMENT_DESC Mesh::inputLayout[3] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
};

bool Mesh::copyVertexData(uint8_t* dest, size_t elemSize, size_t stride, size_t count,
    const tinygltf::Model& model, int accessorIndex)
{
    if (accessorIndex < 0 || accessorIndex >= int(model.accessors.size())) return false;

    const auto& acc = model.accessors[accessorIndex];
    const auto& bufView = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bufView.buffer];

    size_t offset = bufView.byteOffset + acc.byteOffset;
    size_t bufStride = bufView.byteStride ? bufView.byteStride : elemSize;

    for (size_t i = 0; i < count; ++i)
        std::memcpy(dest + i * stride, buf.data.data() + offset + i * bufStride, elemSize);

    return true;
}

void Mesh::load(const tinygltf::Primitive& prim, const tinygltf::Model& model)
{
    materialID = prim.material;

    auto posIt = prim.attributes.find("POSITION");
    if (posIt == prim.attributes.end()) return;

    const auto& posAcc = model.accessors[posIt->second];
    vertexCount = uint32_t(posAcc.count);
    vertices.resize(vertexCount);

    copyVertexData(reinterpret_cast<uint8_t*>(vertices.data()) + offsetof(Vertex, position),
        sizeof(Vector3), sizeof(Vertex), vertexCount, model, posIt->second);

    auto texIt = prim.attributes.find("TEXCOORD_0");
    if (texIt != prim.attributes.end())
        copyVertexData(reinterpret_cast<uint8_t*>(vertices.data()) + offsetof(Vertex, texCoord),
            sizeof(Vector2), sizeof(Vertex), vertexCount, model, texIt->second);
    else
        for (auto& v : vertices) v.texCoord = Vector2(0, 0);

    auto normIt = prim.attributes.find("NORMAL");
    if (normIt != prim.attributes.end())
        copyVertexData(reinterpret_cast<uint8_t*>(vertices.data()) + offsetof(Vertex, normal),
            sizeof(Vector3), sizeof(Vertex), vertexCount, model, normIt->second);
    else
        for (auto& v : vertices) v.normal = Vector3(0, 0, 1);

    if (prim.indices >= 0)
    {
        const auto& acc = model.accessors[prim.indices];
        indexCount = uint32_t(acc.count);

        if (acc.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT)
        {
            indices.resize(indexCount);
            copyVertexData(reinterpret_cast<uint8_t*>(indices.data()), sizeof(uint32_t), sizeof(uint32_t), indexCount, model, prim.indices);
        }
        else if (acc.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT)
        {
            std::vector<uint16_t> temp(indexCount);
            copyVertexData(reinterpret_cast<uint8_t*>(temp.data()), sizeof(uint16_t), sizeof(uint16_t), indexCount, model, prim.indices);
            indices.resize(indexCount);
            for (uint32_t i = 0; i < indexCount; ++i) indices[i] = temp[i];
        }
    }

    setupGPUBuffers();
}

void Mesh::setupGPUBuffers()
{
    auto* res = app->getResources();
    if (!res) return;

    if (!vertices.empty())
    {
        vertexBuffer = res->createDefaultBuffer(vertices.data(),
            vertices.size() * sizeof(Vertex),
            "MeshVB");
        vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        vbView.StrideInBytes = sizeof(Vertex);
        vbView.SizeInBytes = uint32_t(vertices.size() * sizeof(Vertex));
    }

    if (!indices.empty())
    {
        indexBuffer = res->createDefaultBuffer(indices.data(),
            indices.size() * sizeof(uint32_t),
            "MeshIB");
        ibView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
        ibView.Format = DXGI_FORMAT_R32_UINT;
        ibView.SizeInBytes = uint32_t(indices.size() * sizeof(uint32_t));
    }
}

void Mesh::render(ID3D12GraphicsCommandList* cmdList) const
{
    if (vertexCount == 0) return;

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &vbView);

    if (indexBuffer)
    {
        cmdList->IASetIndexBuffer(&ibView);
        cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
    }
    else
        cmdList->DrawInstanced(vertexCount, 1, 0, 0);
}
