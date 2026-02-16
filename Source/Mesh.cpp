#include "Globals.h"
#include "Mesh.h"
#include "Application.h"
#include "ModuleResources.h"

const D3D12_INPUT_ELEMENT_DESC Mesh::InputLayout[3] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

void Mesh::setData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, int materialIndex)
{
    m_vertices = vertices;
    m_indices = indices;
    m_materialIndex = materialIndex;

    createBuffers();
}

void Mesh::createBuffers()
{
    ModuleResources* resources = app->getResources();
    if (!resources || m_vertices.empty())
        return;

    m_vertexBuffer = resources->createDefaultBuffer(m_vertices.data(), m_vertices.size() * sizeof(Vertex), "MeshVB");

    if (m_vertexBuffer)
    {
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = (UINT)(m_vertices.size() * sizeof(Vertex));
    }

    if (!m_indices.empty())
    {
        m_indexBuffer = resources->createDefaultBuffer(m_indices.data(), m_indices.size() * sizeof(uint32_t), "MeshIB");

        if (m_indexBuffer)
        {
            m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
            m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
            m_indexBufferView.SizeInBytes = (UINT)(m_indices.size() * sizeof(uint32_t));
        }
    }
}

void Mesh::draw(ID3D12GraphicsCommandList* cmdList) const
{
    if (!m_vertexBuffer)
        return;

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

    if (m_indexBuffer)
    {
        cmdList->IASetIndexBuffer(&m_indexBufferView);
        cmdList->DrawIndexedInstanced(getIndexCount(), 1, 0, 0, 0);
    }
    else
    {
        cmdList->DrawInstanced(getVertexCount(), 1, 0, 0);
    }
}

void Mesh::cleanup()
{
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_vertices.clear();
    m_indices.clear();
}
