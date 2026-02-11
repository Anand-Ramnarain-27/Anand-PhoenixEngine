#include "Globals.h"
#include "MeshImporter.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include <tiny_gltf.h>

struct MeshHeader
{
    uint32_t vertexCount;
    uint32_t indexCount;
};

bool MeshImporter::Import(
    const tinygltf::Primitive& primitive,
    const tinygltf::Model& model,
    Mesh& outMesh)
{
    outMesh.m_vertices.clear();
    outMesh.m_indices.clear();

    // --- POSITION ---
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end())
        return false;

    const auto& posAcc = model.accessors[posIt->second];
    outMesh.m_vertices.resize(posAcc.count);

    const auto& posView = model.bufferViews[posAcc.bufferView];
    const auto& posBuffer = model.buffers[posView.buffer];

    const uint8_t* posData =
        posBuffer.data.data() + posView.byteOffset + posAcc.byteOffset;

    for (size_t i = 0; i < posAcc.count; ++i)
    {
        memcpy(&outMesh.m_vertices[i].position,
            posData + i * sizeof(Vector3),
            sizeof(Vector3));
    }

    // --- INDICES ---
    if (primitive.indices >= 0)
    {
        const auto& idxAcc = model.accessors[primitive.indices];
        const auto& idxView = model.bufferViews[idxAcc.bufferView];
        const auto& idxBuffer = model.buffers[idxView.buffer];

        const uint8_t* src =
            idxBuffer.data.data() + idxView.byteOffset + idxAcc.byteOffset;

        outMesh.m_indices.resize(idxAcc.count);

        for (size_t i = 0; i < idxAcc.count; ++i)
        {
            outMesh.m_indices[i] =
                idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT
                ? reinterpret_cast<const uint16_t*>(src)[i]
                : reinterpret_cast<const uint32_t*>(src)[i];
        }
    }

    return true;
}

bool MeshImporter::SaveToBinary(
    const Mesh& mesh,
    const char* path)
{
    MeshHeader header;
    header.vertexCount = (uint32_t)mesh.m_vertices.size();
    header.indexCount = (uint32_t)mesh.m_indices.size();

    std::vector<uint8_t> data(
        sizeof(header) +
        header.vertexCount * sizeof(Mesh::Vertex) +
        header.indexCount * sizeof(uint32_t)
    );

    uint8_t* cursor = data.data();
    memcpy(cursor, &header, sizeof(header)); cursor += sizeof(header);
    memcpy(cursor, mesh.m_vertices.data(),
        header.vertexCount * sizeof(Mesh::Vertex));
    cursor += header.vertexCount * sizeof(Mesh::Vertex);
    memcpy(cursor, mesh.m_indices.data(),
        header.indexCount * sizeof(uint32_t));

    return app->getFileSystem()->Save(path, data.data(), data.size());
}

bool MeshImporter::LoadFromBinary(
    Mesh& mesh,
    const char* path)
{
    std::vector<uint8_t> data;
    if (!app->getFileSystem()->Load(path, data))
        return false;

    const uint8_t* cursor = data.data();

    MeshHeader header;
    memcpy(&header, cursor, sizeof(header));
    cursor += sizeof(header);

    mesh.m_vertices.resize(header.vertexCount);
    memcpy(mesh.m_vertices.data(), cursor,
        header.vertexCount * sizeof(Mesh::Vertex));
    cursor += header.vertexCount * sizeof(Mesh::Vertex);

    mesh.m_indices.resize(header.indexCount);
    memcpy(mesh.m_indices.data(), cursor,
        header.indexCount * sizeof(uint32_t));

    mesh.createBuffers(); // your existing D3D12 code
    return true;
}
