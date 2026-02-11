#pragma once
#include "MaterialBinary.h"
#include <tiny_gltf.h>

class MaterialImporter
{
public:
    static bool Import(
        const tinygltf::Material& gltfMaterial,
        const tinygltf::Model& model,
        const char* basePath,
        const char* libraryPath,
        UID& outMaterialUID
    );
};
