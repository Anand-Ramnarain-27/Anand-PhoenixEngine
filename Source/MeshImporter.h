#pragma once
#include <vector>
#include <cstdint>
#include "Mesh.h"

namespace tinygltf
{
    class Model;
    struct Primitive;
}

class MeshImporter
{
public:
    static bool Import(
        const tinygltf::Primitive& primitive,
        const tinygltf::Model& model,
        Mesh& outMesh
    );

    static bool SaveToBinary(
        const Mesh& mesh,
        const char* libraryPath
    );

    static bool LoadFromBinary(
        Mesh& mesh,
        const char* libraryPath
    );
};
