#include "Globals.h"
#include "Mesh.h"
#include "Application.h"
#include "ModuleResources.h"
#include "tiny_gltf.h"

const D3D12_INPUT_ELEMENT_DESC Mesh::InputLayout[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, texCoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
};

bool Mesh::copyAccessorData(uint8_t* dest, size_t elementSize, size_t stride,
    const tinygltf::Model& model, int accessorIndex)
{
    if (accessorIndex < 0 || accessorIndex >= int(model.accessors.size()))
        return false;

    const auto& acc = model.accessors[accessorIndex];
    const auto& view = model.bufferViews[acc.bufferView];
    const auto& buffer = model.buffers[view.buffer];

    const size_t offset = view.byteOffset + acc.byteOffset;
    const size_t sourceStride = view.byteStride > 0 ? view.byteStride : elementSize;
    const uint8_t* source = buffer.data.data() + offset;

    // Fast copy for packed data
    if (sourceStride == elementSize && stride == elementSize)
    {
        std::memcpy(dest, source, elementSize * acc.count);
    }
    else
    {
        for (size_t i = 0; i < acc.count; ++i)
        {
            std::memcpy(dest + i * stride, source + i * sourceStride, elementSize);
        }
    }

    return true;
}

bool Mesh::load(const tinygltf::Primitive& prim, const tinygltf::Model& model)
{
    m_materialID = prim.material;

    auto posIt = prim.attributes.find("POSITION");
    if (posIt == prim.attributes.end())
        return false;

    const auto& posAcc = model.accessors[posIt->second];
    m_vertexCount = uint32_t(posAcc.count);
    m_vertices.resize(m_vertexCount);

    // Load position data
    copyAccessorData(reinterpret_cast<uint8_t*>(m_vertices.data()) + offsetof(Vertex, position),
        sizeof(Vector3), sizeof(Vertex), model, posIt->second);

    // Load texture coordinates
    auto texIt = prim.attributes.find("TEXCOORD_0");
    if (texIt != prim.attributes.end())
    {
        copyAccessorData(reinterpret_cast<uint8_t*>(m_vertices.data()) + offsetof(Vertex, texCoord),
            sizeof(Vector2), sizeof(Vertex), model, texIt->second);
    }
    else
    {
        for (auto& v : m_vertices) v.texCoord = Vector2(0, 0);
    }

    // Load normals
    auto normIt = prim.attributes.find("NORMAL");
    if (normIt != prim.attributes.end())
    {
        copyAccessorData(reinterpret_cast<uint8_t*>(m_vertices.data()) + offsetof(Vertex, normal),
            sizeof(Vector3), sizeof(Vertex), model, normIt->second);
    }
    else
    {
        // Generate flat normals if not present
        for (auto& v : m_vertices) v.normal = Vector3(0, 0, 1);
    }

    // Load indices
    if (prim.indices >= 0)
    {
        const auto& acc = model.accessors[prim.indices];
        m_indexCount = uint32_t(acc.count);

        if (acc.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT)
        {
            m_indices.resize(m_indexCount);
            copyAccessorData(reinterpret_cast<uint8_t*>(m_indices.data()),
                sizeof(uint32_t), sizeof(uint32_t), model, prim.indices);
        }
        else if (acc.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT)
        {
            std::vector<uint16_t> temp(m_indexCount);
            copyAccessorData(reinterpret_cast<uint8_t*>(temp.data()),
                sizeof(uint16_t), sizeof(uint16_t), model, prim.indices);
            m_indices.assign(temp.begin(), temp.end());
        }
    }

    // Calculate tangents if we have texture coordinates but no tangents
    calculateTangents();

    createGPUBuffers();
    return true;
}

void Mesh::calculateTangents()
{
    if (m_vertices.empty() || m_indices.empty())
        return;

    // Simple tangent calculation
    for (size_t i = 0; i < m_indices.size(); i += 3)
    {
        Vertex& v0 = m_vertices[m_indices[i]];
        Vertex& v1 = m_vertices[m_indices[i + 1]];
        Vertex& v2 = m_vertices[m_indices[i + 2]];

        Vector3 edge1 = v1.position - v0.position;
        Vector3 edge2 = v2.position - v0.position;

        Vector2 deltaUV1 = v1.texCoord - v0.texCoord;
        Vector2 deltaUV2 = v2.texCoord - v0.texCoord;

        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

        Vector3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

        tangent.Normalize();

        v0.tangent = tangent;
        v1.tangent = tangent;
        v2.tangent = tangent;
    }
}

void Mesh::createGPUBuffers()
{
    auto* res = app->getResources();
    if (!res) return;

    if (!m_vertices.empty())
    {
        m_vertexBuffer = res->createDefaultBuffer(m_vertices.data(),
            m_vertices.size() * sizeof(Vertex),
            "MeshVB");

        m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vbView.StrideInBytes = sizeof(Vertex);
        m_vbView.SizeInBytes = uint32_t(m_vertices.size() * sizeof(Vertex));
    }

    if (!m_indices.empty())
    {
        m_indexBuffer = res->createDefaultBuffer(m_indices.data(),
            m_indices.size() * sizeof(uint32_t),
            "MeshIB");

        m_ibView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_ibView.Format = DXGI_FORMAT_R32_UINT;
        m_ibView.SizeInBytes = uint32_t(m_indices.size() * sizeof(uint32_t));
    }
}

void Mesh::render(ID3D12GraphicsCommandList* cmdList) const
{
    if (m_vertexCount == 0) return;

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &m_vbView);

    if (m_indexBuffer)
    {
        cmdList->IASetIndexBuffer(&m_ibView);
        cmdList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }
    else
    {
        cmdList->DrawInstanced(m_vertexCount, 1, 0, 0);
    }
}