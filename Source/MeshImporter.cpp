#include "MeshImporter.h"
#include "Application.h"
#include "ModuleFileSystem.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

bool MeshImporter::Import(const tinygltf::Primitive& primitive,
    const tinygltf::Model& model,
    const std::string& outputFile)
{
    // TODO: Move your OLD glTF vertex/index extraction logic here

    std::vector<StaticMesh::Vertex> vertices;
    std::vector<uint32_t> indices;
    int materialIndex = primitive.material;

    // ---- FILL vertices + indices from GLTF ----

    MeshHeader header;
    header.vertexCount = (uint32_t)vertices.size();
    header.indexCount = (uint32_t)indices.size();
    header.materialIndex = materialIndex;

    return Save(header, vertices, indices, outputFile);
}

bool MeshImporter::Save(const MeshHeader& header,
    const std::vector<StaticMesh::Vertex>& vertices,
    const std::vector<uint32_t>& indices,
    const std::string& file)
{
    uint32_t size =
        sizeof(MeshHeader) +
        header.vertexCount * sizeof(StaticMesh::Vertex) +
        header.indexCount * sizeof(uint32_t);

    char* buffer = new char[size];
    char* cursor = buffer;

    memcpy(cursor, &header, sizeof(MeshHeader));
    cursor += sizeof(MeshHeader);

    memcpy(cursor,
        vertices.data(),
        header.vertexCount * sizeof(StaticMesh::Vertex));
    cursor += header.vertexCount * sizeof(StaticMesh::Vertex);

    memcpy(cursor,
        indices.data(),
        header.indexCount * sizeof(uint32_t));

    bool result = app->getFileSystem()->Save(file.c_str(), buffer, size);

    delete[] buffer;
    return result;
}

bool MeshImporter::Load(const std::string& file,
    std::unique_ptr<StaticMesh>& outMesh)
{
    char* buffer = nullptr;

    uint32_t size =
        app->getFileSystem()->Load(file.c_str(), &buffer);

    if (!buffer || size == 0)
        return false;

    char* cursor = buffer;

    MeshHeader header;
    memcpy(&header, cursor, sizeof(MeshHeader));

    if (header.magic != 0x4853454D)
    {
        delete[] buffer;
        return false;
    }

    cursor += sizeof(MeshHeader);

    std::vector<StaticMesh::Vertex> vertices(header.vertexCount);
    std::vector<uint32_t> indices(header.indexCount);

    memcpy(vertices.data(),
        cursor,
        header.vertexCount * sizeof(StaticMesh::Vertex));
    cursor += header.vertexCount * sizeof(StaticMesh::Vertex);

    memcpy(indices.data(),
        cursor,
        header.indexCount * sizeof(uint32_t));

    delete[] buffer;

    outMesh = std::make_unique<StaticMesh>();
    outMesh->setData(vertices, indices, header.materialIndex);

    return true;
}
