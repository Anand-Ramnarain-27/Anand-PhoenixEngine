#include "Globals.h"
#include "SceneImporter.h"
#include "MeshImporter.h"
#include "MaterialImporter.h"
#include "Model.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "tiny_gltf.h"
#include <filesystem>
#include <cstring>

bool SceneImporter::ImportFromLoadedGLTF(const tinygltf::Model& gltfModel, const std::string& sceneName)
{
    if (!CreateSceneDirectory(sceneName))
    {
        LOG("SceneImporter: Failed to create scene directory for %s", sceneName.c_str());
        return false;
    }

    ModuleFileSystem* fs = app->getFileSystem();
    std::string meshFolder = fs->GetLibraryPath() + "Meshes/" + sceneName;
    std::string basePath = "Assets/Models/" + sceneName + "/";

    int meshIndex = 0;
    for (const auto& mesh : gltfModel.meshes)
        for (const auto& prim : mesh.primitives)
            if (!MeshImporter::Import(prim, gltfModel, meshFolder + "/" + std::to_string(meshIndex++) + ".mesh"))
                LOG("SceneImporter: Failed to import mesh %d", meshIndex - 1);

    std::string matFolder = fs->GetLibraryPath() + "Materials/" + sceneName;
    int matIndex = 0;
    for (const auto& mat : gltfModel.materials)
        MaterialImporter::Import(mat, gltfModel, sceneName, matFolder + "/" + std::to_string(matIndex++) + ".mat", matIndex - 1, basePath);

    if (!SaveSceneMetadata(sceneName, gltfModel))
    {
        LOG("SceneImporter: Failed to save scene metadata");
        return false;
    }

    return true;
}

bool SceneImporter::LoadScene(const std::string& sceneName, std::unique_ptr<Model>& outModel)
{
    ModuleFileSystem* fs = app->getFileSystem();
    std::string folder = fs->GetLibraryPath() + "Meshes/" + sceneName;

    if (!fs->Exists(folder.c_str()))
    {
        LOG("SceneImporter: Scene folder does not exist: %s", folder.c_str());
        return false;
    }

    SceneHeader header;
    if (!LoadSceneMetadata(sceneName, header))
    {
        LOG("SceneImporter: Failed to load metadata for %s", sceneName.c_str());
        return false;
    }

    return true;
}

bool SceneImporter::CreateSceneDirectory(const std::string& sceneName)
{
    ModuleFileSystem* fs = app->getFileSystem();
    std::string lib = fs->GetLibraryPath();
    return fs->CreateDir((lib + "Meshes/" + sceneName).c_str())
        && fs->CreateDir((lib + "Materials/" + sceneName).c_str());
}

bool SceneImporter::SaveSceneMetadata(const std::string& sceneName, const tinygltf::Model& gltfModel)
{
    SceneHeader header;
    for (const auto& mesh : gltfModel.meshes)
        header.meshCount += (uint32_t)mesh.primitives.size();
    header.materialCount = (uint32_t)gltfModel.materials.size();

    ModuleFileSystem* fs = app->getFileSystem();
    std::string path = fs->GetLibraryPath() + "Meshes/" + sceneName + "/scene.meta";

    return fs->Save(path.c_str(), &header, sizeof(header));
}

bool SceneImporter::LoadSceneMetadata(const std::string& sceneName, SceneHeader& header)
{
    ModuleFileSystem* fs = app->getFileSystem();
    std::string path = fs->GetLibraryPath() + "Meshes/" + sceneName + "/scene.meta";

    char* buffer = nullptr;
    uint32_t fileSize = fs->Load(path.c_str(), &buffer);

    if (!buffer || fileSize < sizeof(SceneHeader))
    {
        delete[] buffer;
        return false;
    }

    memcpy(&header, buffer, sizeof(header));
    delete[] buffer;

    if (header.magic != 0x53434E45 || header.version != 1)
    {
        LOG("SceneImporter: Invalid scene metadata");
        return false;
    }

    return true;
}