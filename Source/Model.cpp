#include "Globals.h"
#include "Model.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "ModuleD3D12.h"
#include "SceneImporter.h"
#include "MeshImporter.h"
#include "Mesh.h"
#include "Material.h"

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
    LOG("Model: Loading from library: %s", folder.c_str());

    ModuleFileSystem* fs = app->getFileSystem();

    if (!fs->Exists(folder.c_str()))
    {
        LOG("Model: Scene folder does not exist: %s", folder.c_str());
        return false;
    }

    std::string metaPath = folder + "/scene.meta";

    char* buffer = nullptr;
    uint32_t fileSize = fs->Load(metaPath.c_str(), &buffer);

    if (!buffer || fileSize < sizeof(SceneImporter::SceneHeader))
    {
        LOG("Model: Failed to load scene metadata");
        if (buffer) delete[] buffer;
        return false;
    }

    SceneImporter::SceneHeader header;
    memcpy(&header, buffer, sizeof(SceneImporter::SceneHeader));
    delete[] buffer;

    if (header.magic != 0x53434E45 || header.version != 1)
    {
        LOG("Model: Invalid scene metadata file");
        return false;
    }

    LOG("Model: Loading scene with %d meshes", header.meshCount);

    m_meshes.clear();

    for (uint32_t i = 0; i < header.meshCount; ++i)
    {
        std::string meshFile = folder + "/" + std::to_string(i) + ".mesh";

        std::unique_ptr<Mesh> mesh;
        if (MeshImporter::Load(meshFile, mesh))
        {
            m_meshes.push_back(std::move(mesh));
            LOG("  Loaded mesh %d (%u vertices, %u indices)",
                i, m_meshes.back()->getVertexCount(), m_meshes.back()->getIndexCount());
        }
        else
        {
            LOG("  Failed to load mesh file: %s", meshFile.c_str());
        }
    }

    LOG("Model: Successfully loaded %d meshes from library", (int)m_meshes.size());

    return !m_meshes.empty();
}

void Model::draw(ID3D12GraphicsCommandList* cmdList)
{
    for (const auto& mesh : m_meshes)
    {
        if (mesh)
        {
            mesh->draw(cmdList);
        }
    }
}