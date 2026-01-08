#include "Globals.h"
#include "Mesh.h"
#include "Application.h"
#include "ModuleResources.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE

#include "tiny_gltf.h"

bool loadVertexData(uint8_t* data, size_t elemSize, size_t stride, size_t count,
    const tinygltf::Model& model, int accessorIndex)
{
    if (accessorIndex < 0 || accessorIndex >= (int)model.accessors.size())
        return false;

    const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
    if (accessor.count != count)
        return false;

    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

    size_t bufferOffset = bufferView.byteOffset + accessor.byteOffset;
    const uint8_t* srcData = buffer.data.data() + bufferOffset;

    size_t bufferStride = bufferView.byteStride;
    if (bufferStride == 0)
    {
        bufferStride = elemSize;
    }

    // Copy data
    for (size_t i = 0; i < count; ++i)
    {
        memcpy(data + i * stride, srcData + i * bufferStride, elemSize);
    }

    return true;
}

const D3D12_INPUT_ELEMENT_DESC Mesh::s_inputLayout[3] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

Mesh::Mesh()
{
}

Mesh::~Mesh()
{
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
}

void Mesh::load(const tinygltf::Primitive& primitive, const tinygltf::Model& model)
{
    m_materialIndex = primitive.material;

    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end())
        return;

    const tinygltf::Accessor& posAcc = model.accessors[posIt->second];
    m_vertexCount = uint32_t(posAcc.count);
    m_vertices.resize(m_vertexCount);

    loadVertexData(
        reinterpret_cast<uint8_t*>(m_vertices.data()) + offsetof(Vertex, position),
        sizeof(Vector3), sizeof(Vertex), m_vertexCount,
        model, posIt->second
    );

    auto texIt = primitive.attributes.find("TEXCOORD_0");
    if (texIt != primitive.attributes.end())
    {
        loadVertexData(
            reinterpret_cast<uint8_t*>(m_vertices.data()) + offsetof(Vertex, texCoord),
            sizeof(Vector2), sizeof(Vertex), m_vertexCount,
            model, texIt->second
        );
    }
    else
    {
        for (size_t i = 0; i < m_vertices.size(); ++i)
        {
            m_vertices[i].texCoord = Vector2(0, 0);
        }
    }

    auto normalIt = primitive.attributes.find("NORMAL");
    if (normalIt != primitive.attributes.end())
    {
        loadVertexData(
            reinterpret_cast<uint8_t*>(m_vertices.data()) + offsetof(Vertex, normal),
            sizeof(Vector3), sizeof(Vertex), m_vertexCount,
            model, normalIt->second
        );
    }
    else
    {
        for (size_t i = 0; i < m_vertices.size(); ++i)
        {
            m_vertices[i].normal = Vector3(0, 0, 1);
        }
    }

    if (primitive.indices >= 0)
    {
        const tinygltf::Accessor& indAcc = model.accessors[primitive.indices];
        m_indexCount = uint32_t(indAcc.count);

        if (indAcc.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT)
        {
            m_indices.resize(m_indexCount);
            loadVertexData(
                reinterpret_cast<uint8_t*>(m_indices.data()),
                sizeof(uint32_t), sizeof(uint32_t), m_indexCount,
                model, primitive.indices
            );
        }
        else if (indAcc.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT)
        {
            std::vector<uint16_t> shortIndices(m_indexCount);
            loadVertexData(
                reinterpret_cast<uint8_t*>(shortIndices.data()),
                sizeof(uint16_t), sizeof(uint16_t), m_indexCount,
                model, primitive.indices
            );

            m_indices.resize(m_indexCount);
            for (uint32_t i = 0; i < m_indexCount; ++i)
            {
                m_indices[i] = shortIndices[i];
            }
        }
    }

    createBuffers();
}

void Mesh::createBuffers()
{
    ModuleResources* resources = app->getResources();
    if (!resources) return;

    if (!m_vertices.empty())
    {
        m_vertexBuffer = resources->createDefaultBuffer(
            m_vertices.data(),
            m_vertices.size() * sizeof(Vertex),
            "MeshVB"
        );

        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = uint32_t(m_vertices.size() * sizeof(Vertex));
    }

    if (!m_indices.empty())
    {
        m_indexBuffer = resources->createDefaultBuffer(
            m_indices.data(),
            m_indices.size() * sizeof(uint32_t),
            "MeshIB"
        );

        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        m_indexBufferView.SizeInBytes = uint32_t(m_indices.size() * sizeof(uint32_t));
    }
}

void Mesh::draw(ID3D12GraphicsCommandList* commandList) const
{
    if (m_vertexCount == 0) return;

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

    if (m_indexBuffer)
    {
        commandList->IASetIndexBuffer(&m_indexBufferView);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }
    else
    {
        commandList->DrawInstanced(m_vertexCount, 1, 0, 0);
    }
}