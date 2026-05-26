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
    for (size_t i = 0; i < count; ++i) setFn(i, reinterpret_cast<const float*>(data + i * stride));
}

namespace {
struct SkinDataHeader {
    uint32_t magic       = 0x534B494E; // 'SKIN'
    uint32_t version     = 1;
    uint32_t vertexCount = 0;
    uint32_t pad         = 0;
};

static bool SaveSkin(uint32_t vertexCount, const std::vector<Mesh::BoneWeight>& bw, const std::string& meshFile) {
    std::string skinFile = meshFile.substr(0, meshFile.rfind('.')) + ".skin";
    SkinDataHeader hdr; hdr.vertexCount = vertexCount;
    std::vector<char> payload(vertexCount * sizeof(Mesh::BoneWeight));
    memcpy(payload.data(), bw.data(), payload.size());
    return ImporterUtils::SaveBuffer(skinFile, hdr, payload);
}

static bool LoadSkin(const std::string& meshFile, std::vector<Mesh::BoneWeight>& outBW) {
    std::string skinFile = meshFile.substr(0, meshFile.rfind('.')) + ".skin";
    SkinDataHeader hdr;
    std::vector<char> raw;
    if (!ImporterUtils::LoadBuffer(skinFile, hdr, raw)) return false;
    if (hdr.magic != 0x534B494E || hdr.version == 0 || hdr.vertexCount == 0) return false;
    size_t expected = sizeof(SkinDataHeader) + hdr.vertexCount * sizeof(Mesh::BoneWeight);
    if (raw.size() != expected) return false;
    outBW.resize(hdr.vertexCount);
    memcpy(outBW.data(), raw.data() + sizeof(SkinDataHeader), hdr.vertexCount * sizeof(Mesh::BoneWeight));
    return true;
}

struct MorphDataHeader {
    uint32_t magic       = 0x4850524D; // 'MRPH'
    uint32_t version     = 1;
    uint32_t numTargets  = 0;
    uint32_t vertexCount = 0;
};

static bool SaveMorph(uint32_t numTargets, uint32_t vertexCount,
                      const std::vector<Mesh::MorphVertex>& verts, const std::string& meshFile) {
    std::string morphFile = meshFile.substr(0, meshFile.rfind('.')) + ".morph";
    MorphDataHeader hdr; hdr.numTargets = numTargets; hdr.vertexCount = vertexCount;
    std::vector<char> payload(verts.size() * sizeof(Mesh::MorphVertex));
    memcpy(payload.data(), verts.data(), payload.size());
    return ImporterUtils::SaveBuffer(morphFile, hdr, payload);
}

static bool LoadMorph(const std::string& meshFile,
                      std::vector<Mesh::MorphTarget>& outTargets,
                      std::vector<Mesh::MorphVertex>& outVerts) {
    std::string morphFile = meshFile.substr(0, meshFile.rfind('.')) + ".morph";
    MorphDataHeader hdr;
    std::vector<char> raw;
    if (!ImporterUtils::LoadBuffer(morphFile, hdr, raw)) return false;
    if (hdr.magic != 0x4850524D || hdr.version == 0 || hdr.numTargets == 0 || hdr.vertexCount == 0) return false;
    const size_t expected = sizeof(MorphDataHeader) + hdr.numTargets * hdr.vertexCount * sizeof(Mesh::MorphVertex);
    if (raw.size() != expected) return false;
    outTargets.resize(hdr.numTargets);
    outVerts.resize(hdr.numTargets * hdr.vertexCount);
    memcpy(outVerts.data(), raw.data() + sizeof(MorphDataHeader), outVerts.size() * sizeof(Mesh::MorphVertex));
    return true;
}
} // namespace

bool MeshImporter::Import(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const std::string& outputFile) {
    if (!primitive.attributes.count("POSITION")) return false;
    const auto& posAcc = model.accessors[primitive.attributes.at("POSITION")];
    size_t vertexCount = posAcc.count;
    std::vector<Mesh::Vertex> vertices(vertexCount);
    readAccessor<float>(model, posAcc, sizeof(float) * 3, vertexCount, [&](size_t i, const float* e) { vertices[i].position = { e[0], e[1], e[2] }; });
    if (primitive.attributes.count("NORMAL")) readAccessor<float>(model, model.accessors[primitive.attributes.at("NORMAL")], sizeof(float) * 3, vertexCount, [&](size_t i, const float* e) { vertices[i].normal = { e[0], e[1], e[2] }; });
    if (primitive.attributes.count("TEXCOORD_0")) readAccessor<float>(model, model.accessors[primitive.attributes.at("TEXCOORD_0")], sizeof(float) * 2, vertexCount, [&](size_t i, const float* e) { vertices[i].texCoord = { e[0], e[1] }; });
    if (primitive.attributes.count("TANGENT")) readAccessor<float>(model, model.accessors[primitive.attributes.at("TANGENT")], sizeof(float) * 4, vertexCount, [&](size_t i, const float* e) { vertices[i].tangent = { e[0], e[1], e[2], e[3] }; });
    std::vector<uint32_t> indices;
    if (primitive.indices >= 0) {
        const auto& acc = model.accessors[primitive.indices];
        const unsigned char* data = accessorData(model, acc);
        const int type = acc.componentType;
        size_t stride = acc.ByteStride(model.bufferViews[acc.bufferView]);
        if (!stride) stride = (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) ? sizeof(uint16_t) : sizeof(uint32_t);
        indices.resize(acc.count);
        for (size_t i = 0; i < acc.count; ++i) {
            const unsigned char* e = data + i * stride;
            if (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                indices[i] = *reinterpret_cast<const uint8_t*>(e);  
            else if (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                indices[i] = *reinterpret_cast<const uint16_t*>(e);
            else if (type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                indices[i] = *reinterpret_cast<const uint32_t*>(e);
            else { LOG("MeshImporter: Unsupported index format"); return false; }
        }
    }
    // JOINTS_0 + WEIGHTS_0 — per-vertex skinning data
    std::vector<Mesh::BoneWeight> boneWeights;
    if (primitive.attributes.count("JOINTS_0") && primitive.attributes.count("WEIGHTS_0")) {
        boneWeights.resize(vertexCount);

        // JOINTS_0: VEC4 of UNSIGNED_BYTE or UNSIGNED_SHORT
        const auto& jointsAcc = model.accessors[primitive.attributes.at("JOINTS_0")];
        const unsigned char* jData = accessorData(model, jointsAcc);
        const bool isUByte = (jointsAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);
        const size_t jDefStride = isUByte ? 4 : 8;
        const size_t jStride = accessorStride(model, jointsAcc, jDefStride);
        for (size_t i = 0; i < vertexCount; ++i) {
            const unsigned char* e = jData + i * jStride;
            if (isUByte) {
                boneWeights[i].indices[0] = e[0]; boneWeights[i].indices[1] = e[1];
                boneWeights[i].indices[2] = e[2]; boneWeights[i].indices[3] = e[3];
            } else {
                const uint16_t* us = reinterpret_cast<const uint16_t*>(e);
                boneWeights[i].indices[0] = us[0]; boneWeights[i].indices[1] = us[1];
                boneWeights[i].indices[2] = us[2]; boneWeights[i].indices[3] = us[3];
            }
        }

        // WEIGHTS_0: VEC4 of FLOAT
        readAccessor<float>(model, model.accessors[primitive.attributes.at("WEIGHTS_0")],
            sizeof(float) * 4, vertexCount, [&](size_t i, const float* e) {
                boneWeights[i].weights[0] = e[0]; boneWeights[i].weights[1] = e[1];
                boneWeights[i].weights[2] = e[2]; boneWeights[i].weights[3] = e[3];
            });
    }

    // Morph targets — packed flat: [Target0_V0..Target0_Vn | Target1_V0..Target1_Vn | ...]
    std::vector<Mesh::MorphVertex> morphVerts;
    const uint32_t numMorphTargets = (uint32_t)primitive.targets.size();
    if (numMorphTargets > 0) {
        morphVerts.resize(numMorphTargets * vertexCount);
        for (uint32_t t = 0; t < numMorphTargets; ++t) {
            const auto& target = primitive.targets[t];
            const size_t base = t * vertexCount;
            if (target.count("POSITION"))
                readAccessor<float>(model, model.accessors[target.at("POSITION")], sizeof(float) * 3, vertexCount,
                    [&](size_t i, const float* e) { morphVerts[base + i].deltaPosition = { e[0], e[1], e[2] }; });
            if (target.count("NORMAL"))
                readAccessor<float>(model, model.accessors[target.at("NORMAL")], sizeof(float) * 3, vertexCount,
                    [&](size_t i, const float* e) { morphVerts[base + i].deltaNormal = { e[0], e[1], e[2] }; });
            if (target.count("TANGENT"))
                readAccessor<float>(model, model.accessors[target.at("TANGENT")], sizeof(float) * 3, vertexCount,
                    [&](size_t i, const float* e) { morphVerts[base + i].deltaTangent = { e[0], e[1], e[2] }; });
        }
    }

    MeshHeader header;
    header.vertexCount = (uint32_t)vertices.size();
    header.indexCount = (uint32_t)indices.size();
    header.materialIndex = primitive.material;

    if (header.materialIndex < 0 || header.materialIndex >= (int)model.materials.size())
    {
        header.materialIndex = -1;
    }
    bool ok = Save(header, vertices, indices, outputFile);
    if (ok && !boneWeights.empty())
        SaveSkin(header.vertexCount, boneWeights, outputFile);
    if (ok && numMorphTargets > 0)
        SaveMorph(numMorphTargets, header.vertexCount, morphVerts, outputFile);
    return ok;
}

static bool LoadRaw(const std::string& file, MeshImporter::MeshHeader& header, std::vector<Mesh::Vertex>& vertices, std::vector<uint32_t>& indices) {
    std::vector<char> rawBuffer;
    if (!ImporterUtils::LoadBuffer(file, header, rawBuffer)) return false;
    if (!ImporterUtils::ValidateHeader(header, 0x4853454D)) return false;
    const uint32_t vertexSize = (header.version >= 2) ? sizeof(Mesh::Vertex) : (sizeof(float) * 8);
    uint32_t expected = sizeof(MeshImporter::MeshHeader) + header.vertexCount * vertexSize + header.indexCount * sizeof(uint32_t);
    if (expected != (uint32_t)rawBuffer.size()) { LOG("MeshImporter: Size mismatch loading %s (expected %u, got %u)", file.c_str(), expected, (uint32_t)rawBuffer.size()); return false; }
    const char* cursor = rawBuffer.data() + sizeof(MeshImporter::MeshHeader);
    vertices.resize(header.vertexCount);
    if (header.version >= 2) { memcpy(vertices.data(), cursor, header.vertexCount * sizeof(Mesh::Vertex)); cursor += header.vertexCount * sizeof(Mesh::Vertex); }
    else {
        struct OldVertex { Vector3 position; Vector2 texCoord; Vector3 normal; };
        const OldVertex* old = reinterpret_cast<const OldVertex*>(cursor);
        for (uint32_t i = 0; i < header.vertexCount; ++i) { vertices[i].position = old[i].position; vertices[i].texCoord = old[i].texCoord; vertices[i].normal = old[i].normal; vertices[i].tangent = { 1.f, 0.f, 0.f, 1.f }; }
        cursor += header.vertexCount * sizeof(OldVertex);
    }
    indices.resize(header.indexCount);
    memcpy(indices.data(), cursor, header.indexCount * sizeof(uint32_t));
    return true;
}

bool MeshImporter::Load(const std::string& file, ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer, std::unique_ptr<Mesh>& outMesh) {
    if (!cmd || !staticBuffer) { LOG("MeshImporter::Load: cmd and staticBuffer must not be null"); return false; }
    MeshHeader header;
    std::vector<Mesh::Vertex> vertices;
    std::vector<uint32_t> indices;
    if (!LoadRaw(file, header, vertices, indices)) return false;
    outMesh = std::make_unique<Mesh>();
    outMesh->setData(cmd, staticBuffer, vertices, indices, header.materialIndex);
    std::vector<Mesh::BoneWeight> boneWeights;
    if (LoadSkin(file, boneWeights))
        outMesh->setBoneWeights(cmd, staticBuffer, boneWeights);
    std::vector<Mesh::MorphTarget> morphTargets;
    std::vector<Mesh::MorphVertex> morphVerts;
    if (LoadMorph(file, morphTargets, morphVerts))
        outMesh->setMorphTargets(morphTargets, morphVerts);
    return true;
}

bool MeshImporter::Load(const std::string& file, std::unique_ptr<Mesh>& outMesh) {
    MeshHeader header;
    std::vector<Mesh::Vertex> vertices;
    std::vector<uint32_t> indices;
    if (!LoadRaw(file, header, vertices, indices)) return false;
    outMesh = std::make_unique<Mesh>();
    outMesh->setData(vertices, indices, header.materialIndex);
    std::vector<Mesh::BoneWeight> boneWeights;
    if (LoadSkin(file, boneWeights))
        outMesh->setBoneWeights(nullptr, nullptr, boneWeights);  // CPU-side only; GPU upload deferred to uploadToGPU
    std::vector<Mesh::MorphTarget> morphTargets;
    std::vector<Mesh::MorphVertex> morphVerts;
    if (LoadMorph(file, morphTargets, morphVerts))
        outMesh->setMorphTargets(morphTargets, morphVerts);
    return true;
}

bool MeshImporter::Save(const MeshHeader& header, const std::vector<Mesh::Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& file) {
    std::vector<char> payload(header.vertexCount * sizeof(Mesh::Vertex) + header.indexCount * sizeof(uint32_t));
    char* cursor = payload.data();
    memcpy(cursor, vertices.data(), header.vertexCount * sizeof(Mesh::Vertex));
    cursor += header.vertexCount * sizeof(Mesh::Vertex);
    memcpy(cursor, indices.data(), header.indexCount * sizeof(uint32_t));
    return ImporterUtils::SaveBuffer(file, header, payload);
}