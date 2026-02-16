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
    LOG("SceneImporter: Importing scene %s", sceneName.c_str());

    if (!CreateSceneDirectory(sceneName))
    {
        LOG("SceneImporter: Failed to create scene directory for %s", sceneName.c_str());
        return false;
    }

    ModuleFileSystem* fs = app->getFileSystem();
    std::string meshFolder = fs->GetLibraryPath() + "Meshes/" + sceneName;

    int meshIndex = 0;
    for (const auto& mesh : gltfModel.meshes)
    {
        for (const auto& primitive : mesh.primitives)
        {
            std::string meshFile = meshFolder + "/" + std::to_string(meshIndex++) + ".mesh";

            if (!MeshImporter::Import(primitive, gltfModel, meshFile))
            {
                LOG("SceneImporter: Failed to import mesh %d", meshIndex - 1);
            }
        }
    }

    LOG("SceneImporter: Imported %d meshes", meshIndex);

    std::string materialFolder = fs->GetLibraryPath() + "Materials/" + sceneName;
    std::string basePath = "Assets/Models/" + sceneName + "/"; 

    int materialIndex = 0;
    for (const auto& material : gltfModel.materials)
    {
        std::string materialFile = materialFolder + "/" + std::to_string(materialIndex) + ".mat";

        if (!MaterialImporter::Import(material, gltfModel, sceneName, materialFile, materialIndex, basePath)) 
        {
            LOG("SceneImporter: Failed to import material %d", materialIndex);
        }
        else
        {
            LOG("SceneImporter: Successfully imported material %d", materialIndex);
        }

        materialIndex++;
    }

    LOG("SceneImporter: Processed %d materials", materialIndex);

    if (!SaveSceneMetadata(sceneName, gltfModel))
    {
        LOG("SceneImporter: Failed to save scene metadata");
        return false;
    }

    LOG("SceneImporter: Successfully imported scene %s", sceneName.c_str());
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
        LOG("SceneImporter: Failed to load scene metadata for %s", sceneName.c_str());
        return false;
    }

    LOG("SceneImporter: Loading scene %s with %d meshes",
        sceneName.c_str(), header.meshCount);

    return true;
}

std::string SceneImporter::GetSceneName(const char* filePath)
{
    namespace fs = std::filesystem;
    return fs::path(filePath).stem().string();
}

bool SceneImporter::CreateSceneDirectory(const std::string& sceneName)
{
    ModuleFileSystem* fs = app->getFileSystem();

    std::string meshFolder = fs->GetLibraryPath() + "Meshes/" + sceneName;
    std::string materialFolder = fs->GetLibraryPath() + "Materials/" + sceneName;

    bool success = true;
    success &= fs->CreateDir(meshFolder.c_str());
    success &= fs->CreateDir(materialFolder.c_str());

    return success;
}

bool SceneImporter::SaveSceneMetadata(const std::string& sceneName, const tinygltf::Model& gltfModel)
{
    ModuleFileSystem* fs = app->getFileSystem();

    SceneHeader header;

    header.meshCount = 0;
    for (const auto& mesh : gltfModel.meshes)
    {
        header.meshCount += (uint32_t)mesh.primitives.size();
    }

    header.materialCount = (uint32_t)gltfModel.materials.size();

    std::string metadataPath = fs->GetLibraryPath() + "Meshes/" + sceneName + "/scene.meta";

    uint32_t bufferSize = sizeof(SceneHeader);
    std::vector<char> buffer(bufferSize);

    memcpy(buffer.data(), &header, sizeof(SceneHeader));

    if (!fs->Save(metadataPath.c_str(), buffer.data(), bufferSize))
    {
        LOG("SceneImporter: Failed to save metadata to %s", metadataPath.c_str());
        return false;
    }

    LOG("SceneImporter: Saved scene metadata: %d meshes, %d materials",
        header.meshCount, header.materialCount);

    return true;
}

bool SceneImporter::LoadSceneMetadata(const std::string& sceneName,
    SceneHeader& header)
{
    ModuleFileSystem* fs = app->getFileSystem();
    std::string metadataPath = fs->GetLibraryPath() + "Meshes/" + sceneName + "/scene.meta";

    char* buffer = nullptr;
    uint32_t fileSize = fs->Load(metadataPath.c_str(), &buffer);

    if (!buffer || fileSize < sizeof(SceneHeader))
    {
        if (buffer) delete[] buffer;
        return false;
    }

    memcpy(&header, buffer, sizeof(SceneHeader));
    delete[] buffer;

    if (header.magic != 0x53434E45 || header.version != 1)
    {
        LOG("SceneImporter: Invalid scene metadata file");
        return false;
    }

    return true;
}