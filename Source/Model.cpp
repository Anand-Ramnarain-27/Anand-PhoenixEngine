#include "Globals.h"
#include "Model.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "SceneImporter.h"
#include "MeshImporter.h"

#include "tiny_gltf.h"
#include <filesystem>


bool Model::load(const char* fileName)
{
    namespace fs = std::filesystem;

    m_srcFile = fileName;

    std::string modelName = fs::path(fileName).stem().string();

    std::string meshFolder =
        app->getFileSystem()->GetLibraryPath() +
        "Meshes/" + modelName + "/";

    if (!app->getFileSystem()->Exists(meshFolder.c_str()))
    {
        auto result = SceneImporter::ImportScene(fileName);
        if (!result.success)
            return false;
    }

    return loadFromLibrary(meshFolder);
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