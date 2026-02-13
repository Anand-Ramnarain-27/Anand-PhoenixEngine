#include "Globals.h"
#include "Model.h"
#include "SceneImporter.h"
#include "MeshImporter.h"
#include "Application.h"
#include "ModuleFileSystem.h"

#include "tiny_gltf.h"
#include <filesystem>

bool Model::load(const char* fileName)
{
    namespace fs = std::filesystem;

    m_srcFile = fileName;

    std::string modelName = fs::path(fileName).stem().string();
    std::string folder = app->getFileSystem()->GetLibraryPath() + "Meshes/" + modelName;

    if (!app->getFileSystem()->Exists(folder.c_str()))
    {
        LOG("Model: Scene not imported yet, importing %s", fileName);
        if (!importFromGLTF(fileName))
        {
            LOG("Model: Failed to import scene from %s", fileName);
            return false;
        }
    }
    else
    {
        LOG("Model: Scene already imported, loading from Library");
    }
    return loadFromLibrary(folder);
}

bool Model::importFromGLTF(const char* fileName)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model gltfModel;
    std::string error, warning;

    LOG("Model: Loading GLTF file %s", fileName);

    if (!loader.LoadASCIIFromFile(&gltfModel, &error, &warning, fileName))
    {
        LOG("Model: Failed to load GLTF file: %s", error.c_str());
        return false;
    }

    if (!warning.empty())
    {
        LOG("Model: GLTF Warning: %s", warning.c_str());
    }

    namespace fs = std::filesystem;
    std::string modelName = fs::path(fileName).stem().string();

    return SceneImporter::ImportFromLoadedGLTF(gltfModel, modelName);
}

bool Model::loadFromLibrary(const std::string& folder)
{
    m_meshes.clear();

    for (int i = 0;; ++i)
    {
        std::string path = folder + "/" + std::to_string(i) + ".mesh";

        if (!app->getFileSystem()->Exists(path.c_str()))
        {
            break;
        }

        std::unique_ptr<Mesh> mesh;
        if (MeshImporter::Load(path, mesh))
        {
            m_meshes.push_back(std::move(mesh));
            LOG("Model: Loaded mesh %d from %s", i, path.c_str());
        }
        else
        {
            LOG("Model: Failed to load mesh from %s", path.c_str());
        }
    }

    if (m_meshes.empty())
    {
        LOG("Model: No meshes loaded from %s", folder.c_str());
        return false;
    }

    LOG("Model: Successfully loaded %d meshes", (int)m_meshes.size());
    return true;
}

void Model::draw(ID3D12GraphicsCommandList* cmdList)
{
    for (const auto& mesh : m_meshes)
    {
        mesh->draw(cmdList);
    }
}