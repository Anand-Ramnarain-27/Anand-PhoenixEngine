#include "Globals.h"
#include "Mesh.h"
#include "Application.h"
#include "ModuleResources.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "tiny_gltf.h"

Mesh::Mesh()
    : uid(GenerateUID()){
}

const D3D12_INPUT_ELEMENT_DESC Mesh::InputLayout[3] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

template<typename T>
void Mesh::copyVertexData(const uint8_t* srcData, size_t srcStride, size_t count, size_t offset)
{
    for (size_t i = 0; i < count; ++i)
    {
        memcpy(reinterpret_cast<uint8_t*>(&m_vertices[i]) + offset,
            srcData + i * srcStride,
            sizeof(T));
    }
}

bool Mesh::loadVertices(const tinygltf::Primitive& primitive, const tinygltf::Model& model)
{
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end())
        return false;

    const auto& posAcc = model.accessors[posIt->second];
    m_vertices.resize(posAcc.count);

    const auto& posBufferView = model.bufferViews[posAcc.bufferView];
    const auto& posBuffer = model.buffers[posBufferView.buffer];
    const uint8_t* posData = posBuffer.data.data() + posBufferView.byteOffset + posAcc.byteOffset;
    size_t posStride = posBufferView.byteStride ? posBufferView.byteStride : sizeof(Vector3);

    copyVertexData<Vector3>(posData, posStride, posAcc.count, offsetof(Vertex, position));

    auto texIt = primitive.attributes.find("TEXCOORD_0");
    if (texIt != primitive.attributes.end())
    {
        const auto& texAcc = model.accessors[texIt->second];
        if (texAcc.count == posAcc.count)
        {
            const auto& texBufferView = model.bufferViews[texAcc.bufferView];
            const auto& texBuffer = model.buffers[texBufferView.buffer];
            const uint8_t* texData = texBuffer.data.data() + texBufferView.byteOffset + texAcc.byteOffset;
            size_t texStride = texBufferView.byteStride ? texBufferView.byteStride : sizeof(Vector2);

            copyVertexData<Vector2>(texData, texStride, texAcc.count, offsetof(Vertex, texCoord));
        }
    }
    else
    {
        for (auto& vertex : m_vertices)
        {
            vertex.texCoord = Vector2::Zero;
        }
    }

    auto normalIt = primitive.attributes.find("NORMAL");
    if (normalIt != primitive.attributes.end())
    {
        const auto& normalAcc = model.accessors[normalIt->second];
        if (normalAcc.count == posAcc.count)
        {
            const auto& normalBufferView = model.bufferViews[normalAcc.bufferView];
            const auto& normalBuffer = model.buffers[normalBufferView.buffer];
            const uint8_t* normalData = normalBuffer.data.data() + normalBufferView.byteOffset + normalAcc.byteOffset;
            size_t normalStride = normalBufferView.byteStride ? normalBufferView.byteStride : sizeof(Vector3);

            copyVertexData<Vector3>(normalData, normalStride, normalAcc.count, offsetof(Vertex, normal));
        }
    }
    else
    {
        for (auto& vertex : m_vertices)
        {
            vertex.normal = Vector3::UnitZ;
        }
    }

    return true;
}

bool Mesh::loadIndices(const tinygltf::Primitive& primitive, const tinygltf::Model& model)
{
    if (primitive.indices < 0)
        return true; 

    const auto& indAcc = model.accessors[primitive.indices];
    m_indices.resize(indAcc.count);

    const auto& bufferView = model.bufferViews[indAcc.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];
    const uint8_t* srcData = buffer.data.data() + bufferView.byteOffset + indAcc.byteOffset;
    size_t stride = bufferView.byteStride ? bufferView.byteStride : tinygltf::GetComponentSizeInBytes(indAcc.componentType);

    switch (indAcc.componentType)
    {
    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
        for (size_t i = 0; i < indAcc.count; ++i)
        {
            m_indices[i] = *reinterpret_cast<const uint32_t*>(srcData + i * stride);
        }
        break;

    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
        for (size_t i = 0; i < indAcc.count; ++i)
        {
            m_indices[i] = *reinterpret_cast<const uint16_t*>(srcData + i * stride);
        }
        break;

    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
        for (size_t i = 0; i < indAcc.count; ++i)
        {
            m_indices[i] = *reinterpret_cast<const uint8_t*>(srcData + i * stride);
        }
        break;

    default:
        LOG("Unsupported index format in mesh");
        m_indices.clear();
        return false;
    }

    return true;
}

bool Mesh::load(const tinygltf::Primitive& primitive, const tinygltf::Model& model)
{
    m_materialIndex = primitive.material;

    if (!loadVertices(primitive, model))
        return false;

    if (!loadIndices(primitive, model))
        return false;

    createBuffers();
    return true;
}

void Mesh::createBuffers()
{
    ModuleResources* resources = app->getResources();
    if (!resources || m_vertices.empty())
        return;

    m_vertexBuffer = resources->createDefaultBuffer(
        m_vertices.data(),
        m_vertices.size() * sizeof(Vertex),
        "MeshVB"
    );

    if (m_vertexBuffer)
    {
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = static_cast<UINT>(m_vertices.size() * sizeof(Vertex));
    }

    if (!m_indices.empty())
    {
        m_indexBuffer = resources->createDefaultBuffer(
            m_indices.data(),
            m_indices.size() * sizeof(uint32_t),
            "MeshIB"
        );

        if (m_indexBuffer)
        {
            m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
            m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
            m_indexBufferView.SizeInBytes = static_cast<UINT>(m_indices.size() * sizeof(uint32_t));
        }
    }
}

void Mesh::cleanup()
{
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_vertices.clear();
    m_indices.clear();
}

void Mesh::draw(ID3D12GraphicsCommandList* commandList) const
{
    if (m_vertices.empty() || !m_vertexBuffer)
        return;

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

    if (m_indexBuffer)
    {
        commandList->IASetIndexBuffer(&m_indexBufferView);
        commandList->DrawIndexedInstanced(getIndexCount(), 1, 0, 0, 0);
    }
    else
    {
        commandList->DrawInstanced(getVertexCount(), 1, 0, 0);
    }
}