#include "Globals.h"
#include "Model.h"
#include "MeshImporter.h"
#include "Application.h"
#include "ModuleFileSystem.h"

#include "tiny_gltf.h"
#include <filesystem>

bool Model::load(const char* fileName)
{
    namespace fs = std::filesystem;

    m_srcFile = fileName;

    std::string modelName =
        fs::path(fileName).stem().string();

    std::string folder =
        app->getFileSystem()->GetLibraryPath() +
        "Meshes/" + modelName;

    if (!app->getFileSystem()->Exists(folder.c_str()))
    {
        app->getFileSystem()->CreateDir(folder.c_str());

        if (!importFromGLTF(fileName))
            return false;
    }

    return loadFromLibrary(folder);
}

bool Model::importFromGLTF(const char* fileName)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model gltfModel;
    std::string error, warning;

    if (!loader.LoadASCIIFromFile(&gltfModel, &error, &warning, fileName))
        return false;

    namespace fs = std::filesystem;
    std::string modelName = fs::path(fileName).stem().string();

    std::string folder =
        app->getFileSystem()->GetLibraryPath() +
        "Meshes/" + modelName;

    int index = 0;

    for (const auto& mesh : gltfModel.meshes)
    {
        for (const auto& primitive : mesh.primitives)
        {
            std::string out =
                folder + "/" +
                std::to_string(index++) + ".mesh";

            MeshImporter::Import(primitive, gltfModel, out);
        }
    }

    return true;
}

bool Model::loadFromLibrary(const std::string& folder)
{
    m_meshes.clear();

    for (int i = 0;; ++i)
    {
        std::string path =
            folder + "/" +
            std::to_string(i) + ".mesh";

        if (!app->getFileSystem()->Exists(path.c_str()))
            break;

        std::unique_ptr<Mesh> mesh;
        if (MeshImporter::Load(path, mesh))
        {
            m_meshes.push_back(std::move(mesh));
        }
    }

    return !m_meshes.empty();
}

void Model::draw(ID3D12GraphicsCommandList* cmdList)
{
    for (const auto& mesh : m_meshes)
        mesh->draw(cmdList);
}
