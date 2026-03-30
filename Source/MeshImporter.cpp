#include "Globals.h"
#include "MeshImporter.h"
#include "Mesh.h"
#include "ModuleStaticBuffer.h"
#include "ImporterUtils.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include <cstring>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

static const unsigned char* accessorData(const tinygltf::Model& model, const tinygltf::Accessor& acc) {
    const auto& view = model.bufferViews[acc.bufferView];
    return model.buffers[view.buffer].data.data() + view.byteOffset + acc.byteOffset;
}

static size_t accessorStride(const tinygltf::Model& model, const tinygltf::Accessor& acc, size_t defaultStride) {
    size_t stride = acc.ByteStride(model.bufferViews[acc.bufferView]);
    return stride ? stride : defaultStride;
}

template<typename T, typename SetFn>
static void readAccessor(const tinygltf::Model& model, const tinygltf::Accessor& acc, size_t defaultStride, size_t count, SetFn setFn) {
    const unsigned char* data = accessorData(model, acc);
    size_t stride = accessorStride(model, acc, defaultStride);
    for (size_t i = 0; i < count; ++i)
        setFn(i, reinterpret_cast<const T*>(data + i * stride));
}

bool MeshImporter::Import(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const std::string& outputFile) {
    if (!primitive.attributes.count("POSITION")) return false;

    const auto& posAcc = model.accessors[primitive.attributes.at("POSITION")];
    size_t vertexCount = posAcc.count;

    std::vector<Mesh::Vertex> vertices(vertexCount);

    readAccessor<float>(model, posAcc, sizeof(float) * 3, vertexCount, [&](size_t i, const float* e) {
        vertices[i].position = { e[0], e[1], e[2] };
        });

    if (primitive.attributes.count("NORMAL"))
        readAccessor<float>(model, model.accessors[primitive.attributes.at("NORMAL")], sizeof(float) * 3, vertexCount, [&](size_t i, const float* e) {
        vertices[i].normal = { e[0], e[1], e[2] };
            });

    if (primitive.attributes.count("TEXCOORD_0"))
        readAccessor<float>(model, model.accessors[primitive.attributes.at("TEXCOORD_0")], sizeof(float) * 2, vertexCount, [&](size_t i, const float* e) {
        vertices[i].texCoord = { e[0], e[1] };
            });

    if (primitive.attributes.count("TANGENT"))
        readAccessor<float>(model, model.accessors[primitive.attributes.at("TANGENT")], sizeof(float) * 4, vertexCount, [&](size_t i, const float* e) {
        vertices[i].tangent = { e[0], e[1], e[2], e[3] };
            });

    std::vector<uint32_t> indices;

    if (primitive.indices >= 0) {
        const auto& acc = model.accessors[primitive.indices];
        const unsigned char* data = accessorData(model, acc);
        const int type = acc.componentType;

        size_t stride = acc.ByteStride(model.bufferViews[acc.bufferView]);
        if (!stride)
            stride = (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) ? sizeof(uint16_t) : sizeof(uint32_t);

        indices.resize(acc.count);

        for (size_t i = 0; i < acc.count; ++i) {
            const unsigned char* e = data + i * stride;

            if (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                indices[i] = *reinterpret_cast<const uint8_t*>(e);
            else if (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                indices[i] = *reinterpret_cast<const uint16_t*>(e);
            else if (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                indices[i] = *reinterpret_cast<const uint32_t*>(e);
            else {
                LOG("MeshImporter: Unsupported index format");
                return false;
            }
        }
    }

    std::vector<Mesh::BoneWeight> skinWeights;

    if (primitive.attributes.count("JOINTS_0") && primitive.attributes.count("WEIGHTS_0")) {
        skinWeights.resize(vertexCount);

        const auto& jAcc = model.accessors[primitive.attributes.at("JOINTS_0")];
        const unsigned char* jData = accessorData(model, jAcc);
        const int jType = jAcc.componentType;

        size_t jStride = accessorStride(model, jAcc, (jType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) ? 4 : 8);

        for (size_t vi = 0; vi < vertexCount; ++vi) {
            const unsigned char* src = jData + vi * jStride;

            if (jType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                skinWeights[vi].indices[0] = src[0];
                skinWeights[vi].indices[1] = src[1];
                skinWeights[vi].indices[2] = src[2];
                skinWeights[vi].indices[3] = src[3];
            }
            else {
                const uint16_t* s16 = reinterpret_cast<const uint16_t*>(src);
                skinWeights[vi].indices[0] = s16[0];
                skinWeights[vi].indices[1] = s16[1];
                skinWeights[vi].indices[2] = s16[2];
                skinWeights[vi].indices[3] = s16[3];
            }
        }

        const auto& wAcc = model.accessors[primitive.attributes.at("WEIGHTS_0")];

        size_t wStride = accessorStride(model, wAcc, sizeof(float) * 4);

        readAccessor<float>(model, wAcc, wStride, vertexCount, [&](size_t i, const float* e) {
            skinWeights[i].weights[0] = e[0];
            skinWeights[i].weights[1] = e[1];
            skinWeights[i].weights[2] = e[2];
            skinWeights[i].weights[3] = e[3];
            });
    }

    std::vector<Mesh::Vertex> morphDeltaVerts;
    uint32_t numMorphTargets = (uint32_t)primitive.targets.size();

    for (uint32_t ti = 0; ti < numMorphTargets; ++ti) {
        const auto& target = primitive.targets[ti];
        std::vector<Mesh::Vertex> delta(vertexCount);

        if (target.count("POSITION")) {
            const auto& acc = model.accessors[target.at("POSITION")];
            size_t stride = accessorStride(model, acc, sizeof(float) * 3);
            readAccessor<float>(model, acc, stride, vertexCount,
                [&](size_t i, const float* e) {
                    delta[i].position = { e[0], e[1], e[2] };
                });
        }
        if (target.count("NORMAL")) {
            const auto& acc = model.accessors[target.at("NORMAL")];
            size_t stride = accessorStride(model, acc, sizeof(float) * 3);
            readAccessor<float>(model, acc, stride, vertexCount,
                [&](size_t i, const float* e) {
                    delta[i].normal = { e[0], e[1], e[2] };
                });
        }
        if (target.count("TANGENT")) {
            const auto& acc = model.accessors[target.at("TANGENT")];
            size_t stride = accessorStride(model, acc, sizeof(float) * 3);
            readAccessor<float>(model, acc, stride, vertexCount,
                [&](size_t i, const float* e) {
                    delta[i].tangent = { e[0], e[1], e[2], 0.f };
                });
        }
        morphDeltaVerts.insert(morphDeltaVerts.end(), delta.begin(), delta.end());
    }

    MeshHeader header;
    header.vertexCount = (uint32_t)vertices.size();
    header.indexCount = (uint32_t)indices.size();
    header.materialIndex = primitive.material;
    header.version = 3;
    header.skinWeightCount = (uint32_t)skinWeights.size();
    header.numMorphTargets = numMorphTargets;

    return Save(header, vertices, indices, skinWeights, outputFile);
}

static bool LoadRaw(const std::string& file, MeshImporter::MeshHeader& header, std::vector<Mesh::Vertex>& vertices, std::vector<uint32_t>& indices, std::vector<Mesh::BoneWeight>& skinWeights, std::vector<Mesh::Vertex>& morphDeltas) {
    std::vector<char> rawBuffer;

    if (!ImporterUtils::LoadBuffer(file, header, rawBuffer)) return false;
    if (!ImporterUtils::ValidateHeader(header, 0x4853454D)) return false;

    const char* cursor = rawBuffer.data() + sizeof(MeshImporter::MeshHeader);

    vertices.resize(header.vertexCount);
    memcpy(vertices.data(), cursor, header.vertexCount * sizeof(Mesh::Vertex));
    cursor += header.vertexCount * sizeof(Mesh::Vertex);

    indices.resize(header.indexCount);
    memcpy(indices.data(), cursor, header.indexCount * sizeof(uint32_t));
    cursor += header.indexCount * sizeof(uint32_t);

    if (header.version >= 3 && header.skinWeightCount > 0) {
        skinWeights.resize(header.skinWeightCount);
        memcpy(skinWeights.data(), cursor, header.skinWeightCount * sizeof(Mesh::BoneWeight));
        cursor += header.skinWeightCount * sizeof(Mesh::BoneWeight);
    }

    if (header.numMorphTargets > 0) {
        size_t deltaCount = (size_t)header.numMorphTargets * header.vertexCount;
        morphDeltas.resize(deltaCount);
        memcpy(morphDeltas.data(), cursor, deltaCount * sizeof(Mesh::Vertex));
        cursor += deltaCount * sizeof(Mesh::Vertex);
    }

    return true;
}

bool MeshImporter::Load(const std::string& file, ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer, std::unique_ptr<Mesh>& outMesh) {
    if (!cmd || !staticBuffer) return false;

    MeshHeader header;
    std::vector<Mesh::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Mesh::BoneWeight> skinWeights;
    std::vector<Mesh::Vertex> morphDeltas;

    if (!LoadRaw(file, header, vertices, indices, skinWeights, morphDeltas))
        return false;

    outMesh = std::make_unique<Mesh>();
    outMesh->setData(cmd, staticBuffer, vertices, indices, header.materialIndex);

    if (!skinWeights.empty())
        outMesh->setSkinData(skinWeights, cmd, staticBuffer);

    if (header.numMorphTargets > 0 && !morphDeltas.empty())
        outMesh->setMorphData(morphDeltas, header.numMorphTargets, cmd, staticBuffer);

    return true;
}

bool MeshImporter::Load(const std::string& file, std::unique_ptr<Mesh>& outMesh) {
    MeshHeader header;
    std::vector<Mesh::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Mesh::BoneWeight> skinWeights;
    std::vector<Mesh::Vertex> morphDeltas;

    if (!LoadRaw(file, header, vertices, indices, skinWeights, morphDeltas))
        return false;

    outMesh = std::make_unique<Mesh>();
    outMesh->setData(vertices, indices, header.materialIndex);

    if (!skinWeights.empty())
        outMesh->storeSkinDataCPU(skinWeights);

    if (header.numMorphTargets > 0 && !morphDeltas.empty())
        outMesh->setMorphData(morphDeltas, header.numMorphTargets, nullptr, nullptr);

    return true;
}

bool MeshImporter::Save(const MeshHeader& hdr, const std::vector<Mesh::Vertex>& verts, const std::vector<uint32_t>& idx, const std::vector<Mesh::BoneWeight>& skin, const std::string& file) {
    MeshHeader h = hdr;

    size_t payloadBytes =
        h.vertexCount * sizeof(Mesh::Vertex) +
        h.indexCount * sizeof(uint32_t) +
        h.skinWeightCount * sizeof(Mesh::BoneWeight) +
        (size_t)h.numMorphTargets * h.vertexCount * sizeof(Mesh::Vertex);

    std::vector<char> payload(payloadBytes);

    char* cur = payload.data();

    memcpy(cur, verts.data(), h.vertexCount * sizeof(Mesh::Vertex));
    cur += h.vertexCount * sizeof(Mesh::Vertex);

    memcpy(cur, idx.data(), h.indexCount * sizeof(uint32_t));
    cur += h.indexCount * sizeof(uint32_t);

    if (h.skinWeightCount)
        memcpy(cur, skin.data(), h.skinWeightCount * sizeof(Mesh::BoneWeight));
    cur += h.skinWeightCount * sizeof(Mesh::BoneWeight);

    if (h.numMorphTargets > 0) {
        size_t deltaCount = (size_t)h.numMorphTargets * h.vertexCount;
        const Mesh::Vertex* deltas = reinterpret_cast<const Mesh::Vertex*>(
            reinterpret_cast<const char*>(verts.data()) + h.vertexCount * sizeof(Mesh::Vertex));
        memcpy(cur, deltas, deltaCount * sizeof(Mesh::Vertex));
    }

    return ImporterUtils::SaveBuffer(file, h, payload);
}