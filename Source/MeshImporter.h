#pragma once

#include "Mesh.h"
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <d3d12.h>

class ModuleStaticBuffer;
namespace tinygltf { class Model; struct Primitive; }

class MeshImporter {
public:
    struct MeshHeader {
        uint32_t magic = 0x4853454D;
        uint32_t version = 3;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        int32_t materialIndex = -1;
        uint32_t skinWeightCount = 0;
        uint32_t numMorphTargets = 0;
    };

    static bool Import(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const std::string& outputFile);
    static bool Load(const std::string& file, ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer, std::unique_ptr<Mesh>& outMesh);
    static bool Load(const std::string& file, std::unique_ptr<Mesh>& outMesh);

private:
    static bool Save(const MeshHeader& header, const std::vector<Mesh::Vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<Mesh::BoneWeight>& skin, const std::string& file);
};