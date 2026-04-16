#include "Globals.h"
#include "Mesh.h"
#include "ModuleStaticBuffer.h"
#include "Application.h"
#include "ModuleGPUResources.h"
#include <d3dx12.h>

const D3D12_INPUT_ELEMENT_DESC Mesh::InputLayout[4] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

void Mesh::setData(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, int materialIndex) {
    m_vertices = vertices;
    m_indices = indices;
    m_materialIndex = materialIndex;
    m_hasVertexBuffer = false;
    m_hasIndexBuffer = false;
    if (!staticBuffer || vertices.empty()) { LOG("Mesh::setData(pool): invalid args - skipping GPU upload"); computeAABB(); return; }
    const size_t vbSize = vertices.size() * sizeof(Vertex);
    m_vertexBufferView = staticBuffer->allocVertexBuffer(cmd, vertices.data(), vbSize, sizeof(Vertex), "MeshVB");
    m_hasVertexBuffer = (m_vertexBufferView.BufferLocation != 0);
    if (!indices.empty()) {
        m_indexBufferView = staticBuffer->allocIndexBuffer(cmd, indices.data(), indices.size() * sizeof(uint32_t), DXGI_FORMAT_R32_UINT, "MeshIB");
        m_hasIndexBuffer = (m_indexBufferView.BufferLocation != 0);
    }
    computeAABB();
}

void Mesh::setData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, int materialIndex) {
    m_vertices = vertices;
    m_indices = indices;
    m_materialIndex = materialIndex;
    m_hasVertexBuffer = false;
    m_hasIndexBuffer = false;
    m_legacyVertexBuffer.Reset();
    m_legacyIndexBuffer.Reset();
    computeAABB();
}

void Mesh::uploadToGPU(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer) {
    if (m_hasVertexBuffer || !staticBuffer || m_vertices.empty()) return;
    m_vertexBufferView = staticBuffer->allocVertexBuffer(cmd, m_vertices.data(), m_vertices.size() * sizeof(Vertex), sizeof(Vertex), "MeshVB");
    m_hasVertexBuffer = (m_vertexBufferView.BufferLocation != 0);
    if (!m_indices.empty()) {
        m_indexBufferView = staticBuffer->allocIndexBuffer(cmd, m_indices.data(), m_indices.size() * sizeof(uint32_t), DXGI_FORMAT_R32_UINT, "MeshIB");
        m_hasIndexBuffer = (m_indexBufferView.BufferLocation != 0);
    }
}

void Mesh::draw(ID3D12GraphicsCommandList* cmdList) const {
    if (m_hasVertexBuffer) {
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        if (m_hasIndexBuffer) { cmdList->IASetIndexBuffer(&m_indexBufferView); cmdList->DrawIndexedInstanced(getIndexCount(), 1, 0, 0, 0); }
        else cmdList->DrawInstanced(getVertexCount(), 1, 0, 0);
        return;
    }
    if (!m_legacyVertexBuffer) const_cast<Mesh*>(this)->createLegacyBuffers();
    if (!m_legacyVertexBuffer) return;
    D3D12_VERTEX_BUFFER_VIEW vbv = { m_legacyVertexBuffer->GetGPUVirtualAddress(), (UINT)(m_vertices.size() * sizeof(Vertex)), sizeof(Vertex) };
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &vbv);
    if (m_legacyIndexBuffer) {
        D3D12_INDEX_BUFFER_VIEW ibv = { m_legacyIndexBuffer->GetGPUVirtualAddress(), (UINT)(m_indices.size() * sizeof(uint32_t)), DXGI_FORMAT_R32_UINT };
        cmdList->IASetIndexBuffer(&ibv);
        cmdList->DrawIndexedInstanced(getIndexCount(), 1, 0, 0, 0);
    }
    else cmdList->DrawInstanced(getVertexCount(), 1, 0, 0);
}

void Mesh::createLegacyBuffers() {
    ModuleGPUResources* gpu = app->getGPUResources();
    if (!gpu || m_vertices.empty()) return;
    m_legacyVertexBuffer = gpu->createDefaultBuffer(m_vertices.data(), m_vertices.size() * sizeof(Vertex), "MeshVB_Legacy");
    if (!m_indices.empty()) m_legacyIndexBuffer = gpu->createDefaultBuffer(m_indices.data(), m_indices.size() * sizeof(uint32_t), "MeshIB_Legacy");
}

void Mesh::computeAABB() {
    if (m_vertices.empty()) return;
    m_aabbMin = m_aabbMax = m_vertices[0].position;
    for (const auto& v : m_vertices) { m_aabbMin = Vector3::Min(m_aabbMin, v.position); m_aabbMax = Vector3::Max(m_aabbMax, v.position); }
    m_hasAABB = true;
}