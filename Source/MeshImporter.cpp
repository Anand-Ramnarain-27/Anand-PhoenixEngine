#include "Globals.h"
#include "MeshImporter.h"
#include "Application.h"
#include "ModuleFileSystem.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include <cstring>

static const unsigned char* accessorData(const tinygltf::Model& model, const tinygltf::Accessor& acc)
{
    const auto& view = model.bufferViews[acc.bufferView];
    const auto& buffer = model.buffers[view.buffer];
    return buffer.data.data() + view.byteOffset + acc.byteOffset;
}

static size_t accessorStride(const tinygltf::Model& model, const tinygltf::Accessor& acc, size_t defaultStride)
{
    size_t stride = acc.ByteStride(model.bufferViews[acc.bufferView]);
    return stride ? stride : defaultStride;
}

bool MeshImporter::Import(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const std::string& outputFile)
{
    if (!primitive.attributes.count("POSITION")) return false;

    const auto& posAcc = model.accessors[primitive.attributes.at("POSITION")];
    size_t vertexCount = posAcc.count;

    std::vector<Mesh::Vertex> vertices(vertexCount);

    {
        const unsigned char* data = accessorData(model, posAcc);
        size_t               stride = accessorStride(model, posAcc, sizeof(float) * 3);
        for (size_t i = 0; i < vertexCount; ++i)
        {
            const float* e = reinterpret_cast<const float*>(data + i * stride);
            vertices[i].position = { e[0], e[1], e[2] };
        }
    }

    if (primitive.attributes.count("NORMAL"))
    {
        const auto& acc = model.accessors[primitive.attributes.at("NORMAL")];
        const unsigned char* data = accessorData(model, acc);
        size_t               stride = accessorStride(model, acc, sizeof(float) * 3);
        for (size_t i = 0; i < vertexCount; ++i)
        {
            const float* e = reinterpret_cast<const float*>(data + i * stride);
            vertices[i].normal = { e[0], e[1], e[2] };
        }
    }

    if (primitive.attributes.count("TEXCOORD_0"))
    {
        const auto& acc = model.accessors[primitive.attributes.at("TEXCOORD_0")];
        const unsigned char* data = accessorData(model, acc);
        size_t               stride = accessorStride(model, acc, sizeof(float) * 2);
        for (size_t i = 0; i < vertexCount; ++i)
        {
            const float* e = reinterpret_cast<const float*>(data + i * stride);
            vertices[i].texCoord = { e[0], e[1] };
        }
    }

    std::vector<uint32_t> indices;
    if (primitive.indices >= 0)
    {
        const auto& acc = model.accessors[primitive.indices];
        const unsigned char* data = accessorData(model, acc);
        const int            type = acc.componentType;

        size_t stride = acc.ByteStride(model.bufferViews[acc.bufferView]);
        if (!stride)
        {
            if (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) stride = sizeof(uint16_t);
            else if (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)   stride = sizeof(uint32_t);
        }

        indices.resize(acc.count);
        for (size_t i = 0; i < acc.count; ++i)
        {
            const unsigned char* e = data + i * stride;
            if (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) indices[i] = *reinterpret_cast<const uint16_t*>(e);
            else if (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)   indices[i] = *reinterpret_cast<const uint32_t*>(e);
            else { LOG("MeshImporter: Unsupported index format"); return false; }
        }
    }

    MeshHeader header;
    header.vertexCount = (uint32_t)vertices.size();
    header.indexCount = (uint32_t)indices.size();
    header.materialIndex = primitive.material;

    return Save(header, vertices, indices, outputFile);
}

bool MeshImporter::Load(const std::string& file, std::unique_ptr<Mesh>& outMesh)
{
    char* rawBuffer = nullptr;
    uint32_t fileSize = app->getFileSystem()->Load(file.c_str(), &rawBuffer);

    if (!rawBuffer || fileSize < sizeof(MeshHeader)) return false;

    MeshHeader header;
    memcpy(&header, rawBuffer, sizeof(header));

    if (header.magic != 0x4853454D || header.version != 1)
    {
        delete[] rawBuffer; return false;
    }

    uint32_t expected = sizeof(MeshHeader) + header.vertexCount * sizeof(Mesh::Vertex) + header.indexCount * sizeof(uint32_t);
    if (expected != fileSize)
    {
        delete[] rawBuffer; return false;
    }

    const char* cursor = rawBuffer + sizeof(MeshHeader);

    std::vector<Mesh::Vertex> vertices(header.vertexCount);
    std::vector<uint32_t>     indices(header.indexCount);

    memcpy(vertices.data(), cursor, header.vertexCount * sizeof(Mesh::Vertex));
    cursor += header.vertexCount * sizeof(Mesh::Vertex);
    memcpy(indices.data(), cursor, header.indexCount * sizeof(uint32_t));

    delete[] rawBuffer;

    outMesh = std::make_unique<Mesh>();
    outMesh->setData(vertices, indices, header.materialIndex);
    return true;
}

bool MeshImporter::Save(const MeshHeader& header, const std::vector<Mesh::Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& file)
{
    uint32_t totalSize = sizeof(MeshHeader)
        + header.vertexCount * sizeof(Mesh::Vertex)
        + header.indexCount * sizeof(uint32_t);

    std::vector<char> buffer(totalSize);
    char* cursor = buffer.data();

    memcpy(cursor, &header, sizeof(header));                                   cursor += sizeof(header);
    memcpy(cursor, vertices.data(), header.vertexCount * sizeof(Mesh::Vertex)); cursor += header.vertexCount * sizeof(Mesh::Vertex);
    memcpy(cursor, indices.data(), header.indexCount * sizeof(uint32_t));

    return app->getFileSystem()->Save(file.c_str(), buffer.data(), totalSize);
}