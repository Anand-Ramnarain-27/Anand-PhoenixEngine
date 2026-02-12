#include "Globals.h"
#include "MeshImporter.h"
#include "Application.h"
#include "ModuleFileSystem.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include <cstring>

bool MeshImporter::Import(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const std::string& outputFile)
{
    if (!primitive.attributes.count("POSITION"))
        return false;

    std::vector<Mesh::Vertex> vertices;
    std::vector<uint32_t> indices;

    const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];

    const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];

    const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];

    size_t vertexCount = posAccessor.count;
    vertices.resize(vertexCount);

    size_t posStride = posAccessor.ByteStride(posView);
    if (posStride == 0)
        posStride = sizeof(float) * 3;

    const unsigned char* posData = &posBuffer.data[posView.byteOffset + posAccessor.byteOffset];

    for (size_t i = 0; i < vertexCount; ++i)
    {
        const float* element = reinterpret_cast<const float*>(posData + i * posStride);

        vertices[i].position =
        {
            element[0],
            element[1],
            element[2]
        };
    }

    if (primitive.attributes.count("NORMAL"))
    {
        const tinygltf::Accessor& acc = model.accessors[primitive.attributes.at("NORMAL")];

        const tinygltf::BufferView& view = model.bufferViews[acc.bufferView];

        const tinygltf::Buffer& buffer = model.buffers[view.buffer];

        size_t stride = acc.ByteStride(view);
        if (stride == 0)
            stride = sizeof(float) * 3;

        const unsigned char* data = &buffer.data[view.byteOffset + acc.byteOffset];

        for (size_t i = 0; i < vertexCount; ++i)
        {
            const float* element = reinterpret_cast<const float*>(data + i * stride);

            vertices[i].normal =
            {
                element[0],
                element[1],
                element[2]
            };
        }
    }

    if (primitive.attributes.count("TEXCOORD_0"))
    {
        const tinygltf::Accessor& acc = model.accessors[primitive.attributes.at("TEXCOORD_0")];

        const tinygltf::BufferView& view = model.bufferViews[acc.bufferView];

        const tinygltf::Buffer& buffer = model.buffers[view.buffer];

        size_t stride = acc.ByteStride(view);
        if (stride == 0)
            stride = sizeof(float) * 2;

        const unsigned char* data = &buffer.data[view.byteOffset + acc.byteOffset];

        for (size_t i = 0; i < vertexCount; ++i)
        {
            const float* element = reinterpret_cast<const float*>(data + i * stride);

            vertices[i].texCoord =
            {
                element[0],
                element[1]
            };
        }
    }

    if (primitive.indices >= 0)
    {
        const tinygltf::Accessor& acc = model.accessors[primitive.indices];

        const tinygltf::BufferView& view = model.bufferViews[acc.bufferView];

        const tinygltf::Buffer& buffer = model.buffers[view.buffer];

        size_t stride = acc.ByteStride(view);
        if (stride == 0)
        {
            if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                stride = sizeof(uint16_t);
            else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                stride = sizeof(uint32_t);
        }

        const unsigned char* data = &buffer.data[view.byteOffset + acc.byteOffset];

        indices.resize(acc.count);

        for (size_t i = 0; i < acc.count; ++i)
        {
            const unsigned char* element = data + i * stride;

            if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                indices[i] = *reinterpret_cast<const uint16_t*>(element);
            }
            else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            {
                indices[i] = *reinterpret_cast<const uint32_t*>(element);
            }
            else
            {
                LOG("Unsupported index format");
                return false;
            }
        }
    }

    MeshHeader header;
    header.vertexCount = (uint32_t)vertices.size();
    header.indexCount = (uint32_t)indices.size();
    header.materialIndex = primitive.material;

    LOG("Imported mesh: %u verts, %u indices", header.vertexCount, header.indexCount);

    return Save(header, vertices, indices, outputFile);
}

bool MeshImporter::Load(const std::string& file,std::unique_ptr<Mesh>& outMesh)
{
    char* rawBuffer = nullptr;
    uint32_t fileSize = app->getFileSystem()->Load(file.c_str(), &rawBuffer);

    if (!rawBuffer || fileSize < sizeof(MeshHeader))
        return false;

    char* cursor = rawBuffer;

    MeshHeader header;
    memcpy(&header, cursor, sizeof(MeshHeader));

    if (header.magic != 0x4853454D || header.version != 1)
    {
        delete[] rawBuffer;
        return false;
    }

    uint32_t expectedSize = sizeof(MeshHeader) + header.vertexCount * sizeof(Mesh::Vertex) + header.indexCount * sizeof(uint32_t);

    if (expectedSize != fileSize)
    {
        delete[] rawBuffer;
        return false;
    }

    cursor += sizeof(MeshHeader);

    std::vector<Mesh::Vertex> vertices(header.vertexCount);
    std::vector<uint32_t> indices(header.indexCount);

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
    uint32_t totalSize = sizeof(MeshHeader) + header.vertexCount * sizeof(Mesh::Vertex) + header.indexCount * sizeof(uint32_t);

    std::vector<char> buffer(totalSize);
    char* cursor = buffer.data();

    memcpy(cursor, &header, sizeof(MeshHeader));
    cursor += sizeof(MeshHeader);

    memcpy(cursor, vertices.data(), header.vertexCount * sizeof(Mesh::Vertex));
    cursor += header.vertexCount * sizeof(Mesh::Vertex);

    memcpy(cursor, indices.data(), header.indexCount * sizeof(uint32_t));

    return app->getFileSystem()->Save(file.c_str(), buffer.data(), totalSize);
}
