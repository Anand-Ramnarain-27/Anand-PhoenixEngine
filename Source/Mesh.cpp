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

const D3D12_INPUT_ELEMENT_DESC Mesh::BoneWeightInputLayout[2] = {
    { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT,  1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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

void Mesh::setBoneWeights(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer, const std::vector<BoneWeight>& boneWeights) {
    m_boneWeights = boneWeights;
    m_hasBoneWeightBuffer = false;
    m_boneWeightBuffer.Reset();
    m_boneWeightBufferView = {};
    if (boneWeights.empty()) return;

    // Upload via committed resource (same pattern as morph targets — works without a cmd).
    if (ModuleGPUResources* gpu = app->getGPUResources()) {
        m_boneWeightBuffer = gpu->createDefaultBuffer(
            boneWeights.data(), boneWeights.size() * sizeof(BoneWeight), "MeshBoneWeightVB");
        if (m_boneWeightBuffer) {
            m_boneWeightBufferView.BufferLocation = m_boneWeightBuffer->GetGPUVirtualAddress();
            m_boneWeightBufferView.SizeInBytes    = (UINT)(boneWeights.size() * sizeof(BoneWeight));
            m_boneWeightBufferView.StrideInBytes  = sizeof(BoneWeight);
            m_hasBoneWeightBuffer = true;
            return;
        }
        LOG("Mesh::setBoneWeights: createDefaultBuffer failed — falling back to static buffer");
    }

    // Fallback: static buffer upload (requires a valid cmd).
    if (!cmd || !staticBuffer) return;
    const size_t sz = boneWeights.size() * sizeof(BoneWeight);
    m_boneWeightBufferView = staticBuffer->allocVertexBuffer(cmd, boneWeights.data(), sz, sizeof(BoneWeight), "MeshBoneWeightVB");
    m_hasBoneWeightBuffer = (m_boneWeightBufferView.BufferLocation != 0);
}

void Mesh::setMorphTargets(const std::vector<MorphTarget>& targets, const std::vector<MorphVertex>& vertexData) {
    m_morphTargets    = targets;
    m_morphVertexData = vertexData;
    m_numMorphTargets = (uint32_t)targets.size();
    m_hasMorphTargetBuffer = false;
    m_morphTargetBuffer.Reset();
    if (!vertexData.empty()) {
        if (ModuleGPUResources* gpu = app->getGPUResources()) {
            m_morphTargetBuffer = gpu->createDefaultBuffer(
                vertexData.data(), vertexData.size() * sizeof(MorphVertex), "MorphTargetBuffer");
            m_hasMorphTargetBuffer = (m_morphTargetBuffer != nullptr);
        }
    }
}

D3D12_GPU_VIRTUAL_ADDRESS Mesh::getMorphTargetBufferVA() const {
    return m_morphTargetBuffer ? m_morphTargetBuffer->GetGPUVirtualAddress() : 0;
}

void Mesh::uploadToGPU(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer) {
    if (!staticBuffer) return;
    if (!m_hasVertexBuffer && !m_vertices.empty()) {
        m_vertexBufferView = staticBuffer->allocVertexBuffer(cmd, m_vertices.data(), m_vertices.size() * sizeof(Vertex), sizeof(Vertex), "MeshVB");
        m_hasVertexBuffer = (m_vertexBufferView.BufferLocation != 0);
        if (!m_indices.empty()) {
            m_indexBufferView = staticBuffer->allocIndexBuffer(cmd, m_indices.data(), m_indices.size() * sizeof(uint32_t), DXGI_FORMAT_R32_UINT, "MeshIB");
            m_hasIndexBuffer = (m_indexBufferView.BufferLocation != 0);
        }
    }
    if (!m_hasBoneWeightBuffer && !m_boneWeights.empty()) {
        // Try committed resource first (doesn't need cmd, same as morph targets).
        if (ModuleGPUResources* gpu = app->getGPUResources()) {
            m_boneWeightBuffer = gpu->createDefaultBuffer(
                m_boneWeights.data(), m_boneWeights.size() * sizeof(BoneWeight), "MeshBoneWeightVB");
            if (m_boneWeightBuffer) {
                m_boneWeightBufferView.BufferLocation = m_boneWeightBuffer->GetGPUVirtualAddress();
                m_boneWeightBufferView.SizeInBytes    = (UINT)(m_boneWeights.size() * sizeof(BoneWeight));
                m_boneWeightBufferView.StrideInBytes  = sizeof(BoneWeight);
                m_hasBoneWeightBuffer = true;
            }
        }
        // Fallback: static buffer (only if committed resource failed).
        if (!m_hasBoneWeightBuffer) {
            const size_t sz = m_boneWeights.size() * sizeof(BoneWeight);
            m_boneWeightBufferView = staticBuffer->allocVertexBuffer(cmd, m_boneWeights.data(), sz, sizeof(BoneWeight), "MeshBoneWeightVB");
            m_hasBoneWeightBuffer = (m_boneWeightBufferView.BufferLocation != 0);
            if (!m_hasBoneWeightBuffer)
                LOG("Mesh::uploadToGPU: bone weight upload FAILED via both committed and static buffer");
        }
    }
    if (!m_hasMorphTargetBuffer && !m_morphVertexData.empty()) {
        if (ModuleGPUResources* gpu = app->getGPUResources()) {
            m_morphTargetBuffer = gpu->createDefaultBuffer(
                m_morphVertexData.data(), m_morphVertexData.size() * sizeof(MorphVertex), "MorphTargetBuffer");
            m_hasMorphTargetBuffer = (m_morphTargetBuffer != nullptr);
        }
    }
}

void Mesh::draw(ID3D12GraphicsCommandList* cmdList) const {
    if (m_hasVertexBuffer) {
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        if (m_hasBoneWeightBuffer)
            cmdList->IASetVertexBuffers(1, 1, &m_boneWeightBufferView);
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

void Mesh::drawSkinned(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_VIRTUAL_ADDRESS skinnedVA) const {
    D3D12_VERTEX_BUFFER_VIEW vbv = { skinnedVA, (UINT)(m_vertices.size() * sizeof(Vertex)), sizeof(Vertex) };
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &vbv);
    if (m_hasIndexBuffer) {
        cmdList->IASetIndexBuffer(&m_indexBufferView);
        cmdList->DrawIndexedInstanced(getIndexCount(), 1, 0, 0, 0);
    } else if (m_legacyIndexBuffer) {
        // Static index buffer not yet uploaded — use the legacy buffer created by createLegacyBuffers().
        D3D12_INDEX_BUFFER_VIEW ibv = { m_legacyIndexBuffer->GetGPUVirtualAddress(),
                                        (UINT)(m_indices.size() * sizeof(uint32_t)),
                                        DXGI_FORMAT_R32_UINT };
        cmdList->IASetIndexBuffer(&ibv);
        cmdList->DrawIndexedInstanced(getIndexCount(), 1, 0, 0, 0);
    } else {
        cmdList->DrawInstanced(getVertexCount(), 1, 0, 0);
    }
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