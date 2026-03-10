#include "Globals.h"
#include "MeshImporter.h"
#include "ModuleStaticBuffer.h"
#include "ImporterUtils.h"
#include "Application.h"
#include "ModuleFileSystem.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"
#include <cstring>

static const unsigned char* accessorData(const tinygltf::Model& model, const tinygltf::Accessor& acc){

    const auto& view = model.bufferViews[acc.bufferView];
    return model.buffers[view.buffer].data.data() + view.byteOffset + acc.byteOffset;
}

static size_t accessorStride(const tinygltf::Model& model, const tinygltf::Accessor& acc, size_t defaultStride){

    size_t stride = acc.ByteStride(model.bufferViews[acc.bufferView]);
    return stride ? stride : defaultStride;
}

template<typename T, typename SetFn>
static void readAccessor(const tinygltf::Model& model, const tinygltf::Accessor& acc, size_t defaultStride, size_t count, SetFn setFn){

    const unsigned char* data = accessorData(model, acc);
    size_t               stride = accessorStride(model, acc, defaultStride);
    for (size_t i = 0; i < count; ++i)
        setFn(i, reinterpret_cast<const float*>(data + i * stride));
}

bool MeshImporter::Import(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const std::string& outputFile){

    if (!primitive.attributes.count("POSITION")) return false;

    const auto& posAcc = model.accessors[primitive.attributes.at("POSITION")];
    size_t      vertexCount = posAcc.count;
    std::vector<Mesh::Vertex> vertices(vertexCount);

    readAccessor<float>(model, posAcc, sizeof(float) * 3, vertexCount,
        [&](size_t i, const float* e) { vertices[i].position = { e[0], e[1], e[2] }; });

    if (primitive.attributes.count("NORMAL"))
        readAccessor<float>(model, model.accessors[primitive.attributes.at("NORMAL")],
            sizeof(float) * 3, vertexCount,
            [&](size_t i, const float* e) { vertices[i].normal = { e[0], e[1], e[2] }; });

    if (primitive.attributes.count("TEXCOORD_0"))
        readAccessor<float>(model, model.accessors[primitive.attributes.at("TEXCOORD_0")],
            sizeof(float) * 2, vertexCount,
            [&](size_t i, const float* e) { vertices[i].texCoord = { e[0], e[1] }; });

    if (primitive.attributes.count("TANGENT"))
        readAccessor<float>(model, model.accessors[primitive.attributes.at("TANGENT")],
            sizeof(float) * 4, vertexCount,
            [&](size_t i, const float* e) { vertices[i].tangent = { e[0], e[1], e[2], e[3] }; });

    std::vector<uint32_t> indices;
    if (primitive.indices >= 0) {

        const auto& acc = model.accessors[primitive.indices];
        const unsigned char* data = accessorData(model, acc);
        const int            type = acc.componentType;
        size_t               stride = acc.ByteStride(model.bufferViews[acc.bufferView]);
        if (!stride) stride = (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) ? sizeof(uint16_t) : sizeof(uint32_t);

        indices.resize(acc.count);
        for (size_t i = 0; i < acc.count; ++i){

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

bool MeshImporter::Load(const std::string& file, ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer, std::unique_ptr<Mesh>& outMesh){

    if (!cmd || !staticBuffer)
    {
        LOG("MeshImporter::Load: cmd and staticBuffer must not be null");
        return false;
    }

    MeshHeader header;
    std::vector<char> rawBuffer;

    if (!ImporterUtils::LoadBuffer(file, header, rawBuffer)) return false;
    if (!ImporterUtils::ValidateHeader(header, 0x4853454D)) return false;

    const uint32_t vertexSize = (header.version >= 2) ? sizeof(Mesh::Vertex) : (sizeof(float) * 8);
    uint32_t expected = sizeof(MeshHeader) + header.vertexCount * vertexSize + header.indexCount * sizeof(uint32_t);

    if (expected != (uint32_t)rawBuffer.size()){

        LOG("MeshImporter: Size mismatch loading %s (expected %u, got %u)",
            file.c_str(), expected, (uint32_t)rawBuffer.size());
        return false;
    }

    const char* cursor = rawBuffer.data() + sizeof(MeshHeader);

    std::vector<Mesh::Vertex> vertices(header.vertexCount);

    if (header.version >= 2){

        memcpy(vertices.data(), cursor, header.vertexCount * sizeof(Mesh::Vertex));
        cursor += header.vertexCount * sizeof(Mesh::Vertex);
    }
    else{

        struct OldVertex { Vector3 position; Vector2 texCoord; Vector3 normal; };
        const OldVertex* old = reinterpret_cast<const OldVertex*>(cursor);
        for (uint32_t i = 0; i < header.vertexCount; ++i){
            vertices[i].position = old[i].position;
            vertices[i].texCoord = old[i].texCoord;
            vertices[i].normal = old[i].normal;
            vertices[i].tangent = { 1.f, 0.f, 0.f, 1.f }; 
        }
        cursor += header.vertexCount * sizeof(OldVertex);
    }

    std::vector<uint32_t> indices(header.indexCount);
    memcpy(indices.data(), cursor, header.indexCount * sizeof(uint32_t));

    outMesh = std::make_unique<Mesh>();
    outMesh->setData(cmd, staticBuffer, vertices, indices, header.materialIndex);
    return true;
}

bool MeshImporter::Load(const std::string& file, std::unique_ptr<Mesh>& outMesh){
    MeshHeader        header;
    std::vector<char> rawBuffer;

    if (!ImporterUtils::LoadBuffer(file, header, rawBuffer))    return false;
    if (!ImporterUtils::ValidateHeader(header, 0x4853454D))     return false;

    const uint32_t vertexSize = (header.version >= 2) ? sizeof(Mesh::Vertex) : (sizeof(float) * 8);
    uint32_t expected = sizeof(MeshHeader) + header.vertexCount * vertexSize + header.indexCount * sizeof(uint32_t);

    if (expected != (uint32_t)rawBuffer.size()){
        LOG("MeshImporter: Size mismatch loading %s (expected %u, got %u)",
            file.c_str(), expected, (uint32_t)rawBuffer.size());
        return false;
    }

    const char* cursor = rawBuffer.data() + sizeof(MeshHeader);

    std::vector<Mesh::Vertex> vertices(header.vertexCount);
    if (header.version >= 2){
        memcpy(vertices.data(), cursor, header.vertexCount * sizeof(Mesh::Vertex));
        cursor += header.vertexCount * sizeof(Mesh::Vertex);
    }
    else{
        struct OldVertex { Vector3 position; Vector2 texCoord; Vector3 normal; };
        const OldVertex* old = reinterpret_cast<const OldVertex*>(cursor);
        for (uint32_t i = 0; i < header.vertexCount; ++i){
            vertices[i].position = old[i].position;
            vertices[i].texCoord = old[i].texCoord;
            vertices[i].normal = old[i].normal;
            vertices[i].tangent = { 1.f, 0.f, 0.f, 1.f };
        }
        cursor += header.vertexCount * sizeof(OldVertex);
    }

    std::vector<uint32_t> indices(header.indexCount);
    memcpy(indices.data(), cursor, header.indexCount * sizeof(uint32_t));

    outMesh = std::make_unique<Mesh>();
    outMesh->setData(vertices, indices, header.materialIndex); 
    return true;
}

bool MeshImporter::Save(const MeshHeader& header, const std::vector<Mesh::Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& file){
    std::vector<char> payload(header.vertexCount * sizeof(Mesh::Vertex) + header.indexCount * sizeof(uint32_t));

    char* cursor = payload.data();
    memcpy(cursor, vertices.data(), header.vertexCount * sizeof(Mesh::Vertex));
    cursor += header.vertexCount * sizeof(Mesh::Vertex);
    memcpy(cursor, indices.data(), header.indexCount * sizeof(uint32_t));

    return ImporterUtils::SaveBuffer(file, header, payload);
}