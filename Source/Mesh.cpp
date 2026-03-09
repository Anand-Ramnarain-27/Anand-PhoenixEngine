#include "Globals.h"
#include "Mesh.h"
#include "Application.h"
#include "ModuleGPUResources.h"

const D3D12_INPUT_ELEMENT_DESC Mesh::InputLayout[4] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

void Mesh::setData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, int materialIndex) {
    m_vertices = vertices;
    m_indices = indices;
    m_materialIndex = materialIndex;
    createBuffers();
    computeAABB();
}

void Mesh::computeAABB() {
    if (m_vertices.empty()) return;
    m_aabbMin = m_aabbMax = m_vertices[0].position;
    for (const auto& v : m_vertices) {
        m_aabbMin = Vector3::Min(m_aabbMin, v.position);
        m_aabbMax = Vector3::Max(m_aabbMax, v.position);
    }
    m_hasAABB = true;
}

void Mesh::createBuffers() {
    ModuleGPUResources* gpu = app->getGPUResources();
    if (!gpu || m_vertices.empty()) return;

    m_vertexBuffer = gpu->createDefaultBuffer(m_vertices.data(), m_vertices.size() * sizeof(Vertex), "MeshVB");
    if (m_vertexBuffer) {
        m_vertexBufferView = {
            m_vertexBuffer->GetGPUVirtualAddress(),
            (UINT)(m_vertices.size() * sizeof(Vertex)),
            sizeof(Vertex)
        };
    }

    if (m_indices.empty()) return;

    m_indexBuffer = gpu->createDefaultBuffer(m_indices.data(), m_indices.size() * sizeof(uint32_t), "MeshIB");
    if (m_indexBuffer) {
        m_indexBufferView = {
            m_indexBuffer->GetGPUVirtualAddress(),
            (UINT)(m_indices.size() * sizeof(uint32_t)),
            DXGI_FORMAT_R32_UINT
        };
    }
}

void Mesh::draw(ID3D12GraphicsCommandList* cmdList) const {
    if (!m_vertexBuffer) return;
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    if (m_indexBuffer) {
        cmdList->IASetIndexBuffer(&m_indexBufferView);
        cmdList->DrawIndexedInstanced(getIndexCount(), 1, 0, 0, 0);
    }
    else {
        cmdList->DrawInstanced(getVertexCount(), 1, 0, 0);
    }
}

void Mesh::cleanup() {
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_vertices.clear();
    m_indices.clear();
}