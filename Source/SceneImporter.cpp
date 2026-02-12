#include "Globals.h"
#include "SceneImporter.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "MeshImporter.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include <filesystem>

SceneImporter::ImportResult
SceneImporter::ImportScene(const char* sourceFile)
{
    ImportResult result;

    namespace fs = std::filesystem;

    tinygltf::TinyGLTF loader;
    tinygltf::Model gltfModel;
    std::string error, warning;

    if (!loader.LoadASCIIFromFile(&gltfModel, &error, &warning, sourceFile))
    {
        LOG("SceneImporter: Failed to load GLTF %s", sourceFile);
        return result;
    }

    std::string modelName = fs::path(sourceFile).stem().string();

    ModuleFileSystem* fsys = app->getFileSystem();

    std::string meshFolder =
        fsys->GetLibraryPath() + "Meshes/" + modelName + "/";

    std::string materialFolder =
        fsys->GetLibraryPath() + "Materials/" + modelName + "/";

    fsys->CreateDir(meshFolder.c_str());
    fsys->CreateDir(materialFolder.c_str());

    int meshIndex = 0;

    for (const auto& mesh : gltfModel.meshes)
    {
        for (const auto& primitive : mesh.primitives)
        {
            std::string outPath =
                meshFolder + std::to_string(meshIndex++) + ".mesh";

            MeshImporter::Import(primitive, gltfModel, outPath);
        }
    }

    result.meshesPath = meshFolder;
    result.materialsPath = materialFolder;
    result.success = true;

    return result;
}
