#include "Mesh.h"
#include "Application.h"
#include "ModuleResources.h"
#include "MeshImporter.h"

Mesh::Mesh()
    : uid(GenerateUID())
{
}

Mesh::~Mesh()
{
    cleanup();
}

const D3D12_INPUT_ELEMENT_DESC Mesh::InputLayout[3] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
      D3D12_APPEND_ALIGNED_ELEMENT,
      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
      D3D12_APPEND_ALIGNED_ELEMENT,
      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
      D3D12_APPEND_ALIGNED_ELEMENT,
      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

bool Mesh::loadFromBinary(const char* path)
{
    cleanup();
    return MeshImporter::LoadFromBinary(*this, path);
}

void Mesh::createBuffers()
{
    ModuleResources* resources = app->getResources();
    if (!resources || m_vertices.empty())
        return;

    // Vertex buffer
    m_vertexBuffer = resources->createDefaultBuffer(
        m_vertices.data(),
        m_vertices.size() * sizeof(Vertex),
        "MeshVB"
    );

    if (m_vertexBuffer)
    {
        m_vertexBufferView.BufferLocation =
            m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes =
            (UINT)(m_vertices.size() * sizeof(Vertex));
    }

    // Index buffer
    if (!m_indices.empty())
    {
        m_indexBuffer = resources->createDefaultBuffer(
            m_indices.data(),
            m_indices.size() * sizeof(uint32_t),
            "MeshIB"
        );

        if (m_indexBuffer)
        {
            m_indexBufferView.BufferLocation =
                m_indexBuffer->GetGPUVirtualAddress();
            m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
            m_indexBufferView.SizeInBytes =
                (UINT)(m_indices.size() * sizeof(uint32_t));
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
    if (!m_vertexBuffer || m_vertices.empty())
        return;

    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );

    commandList->IASetVertexBuffers(
        0, 1, &m_vertexBufferView
    );

    if (m_indexBuffer)
    {
        commandList->IASetIndexBuffer(&m_indexBufferView);
        commandList->DrawIndexedInstanced(
            getIndexCount(), 1, 0, 0, 0
        );
    }
    else
    {
        commandList->DrawInstanced(
            getVertexCount(), 1, 0, 0
        );
    }
}
