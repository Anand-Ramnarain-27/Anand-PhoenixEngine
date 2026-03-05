#include "Globals.h"
#include "SceneImporter.h"
#include "MeshImporter.h"
#include "MaterialImporter.h"
#include "ImporterUtils.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "tiny_gltf.h"
#include <filesystem>

bool SceneImporter::ImportFromLoadedGLTF(const tinygltf::Model& gltfModel, const std::string& sceneName) {
    if (!CreateSceneDirectory(sceneName)) { LOG("SceneImporter: Failed to create scene directory for %s", sceneName.c_str()); return false; }

    ModuleFileSystem* fs = app->getFileSystem();
    std::string meshFolder = fs->GetLibraryPath() + "Meshes/" + sceneName;
    std::string basePath = "Assets/Models/" + sceneName + "/";
    std::string matFolder = fs->GetLibraryPath() + "Materials/" + sceneName;

    int meshIndex = 0;
    for (const auto& mesh : gltfModel.meshes)
        for (const auto& prim : mesh.primitives)
            if (!MeshImporter::Import(prim, gltfModel, ImporterUtils::IndexedPath(meshFolder, meshIndex++, ".mesh")))
                LOG("SceneImporter: Failed to import mesh %d", meshIndex - 1);

    int matIndex = 0;
    for (const auto& mat : gltfModel.materials)
        MaterialImporter::Import(mat, gltfModel, sceneName, ImporterUtils::IndexedPath(matFolder, matIndex++, ".mat"), matIndex - 1, basePath);

    if (!SaveSceneMetadata(sceneName, gltfModel)) { LOG("SceneImporter: Failed to save scene metadata"); return false; }
    return true;
}

bool SceneImporter::LoadScene(const std::string& sceneName, std::unique_ptr<Model>& outModel) {
    ModuleFileSystem* fs = app->getFileSystem();
    std::string folder = fs->GetLibraryPath() + "Meshes/" + sceneName;
    if (!fs->Exists(folder.c_str())) { LOG("SceneImporter: Scene folder does not exist: %s", folder.c_str()); return false; }
    SceneHeader header;
    if (!LoadSceneMetadata(sceneName, header)) { LOG("SceneImporter: Failed to load metadata for %s", sceneName.c_str()); return false; }
    return true;
}

bool SceneImporter::CreateSceneDirectory(const std::string& sceneName) {
    ModuleFileSystem* fs = app->getFileSystem();
    std::string lib = fs->GetLibraryPath();
    return fs->CreateDir((lib + "Meshes/" + sceneName).c_str()) && fs->CreateDir((lib + "Materials/" + sceneName).c_str());
}

bool SceneImporter::SaveSceneMetadata(const std::string& sceneName, const tinygltf::Model& gltfModel) {
    SceneHeader header;
    for (const auto& mesh : gltfModel.meshes) header.meshCount += (uint32_t)mesh.primitives.size();
    header.materialCount = (uint32_t)gltfModel.materials.size();
    ModuleFileSystem* fs = app->getFileSystem();
    return ImporterUtils::SaveBuffer(fs->GetLibraryPath() + "Meshes/" + sceneName + "/scene.meta", header);
}

bool SceneImporter::LoadSceneMetadata(const std::string& sceneName, SceneHeader& header) {
    std::vector<char> rawBuffer;
    std::string path = app->getFileSystem()->GetLibraryPath() + "Meshes/" + sceneName + "/scene.meta";
    if (!ImporterUtils::LoadBuffer(path, header, rawBuffer)) return false;
    if (!ImporterUtils::ValidateHeader(header, 0x53434E45)) { LOG("SceneImporter: Invalid scene metadata"); return false; }
    return true;
}