#include "Globals.h"
#include "Model.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "ModuleD3D12.h"
#include "SceneImporter.h"
#include "MeshImporter.h"
#include "MaterialImporter.h"
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

    return loadFromLibrary(folder);
}

bool Model::importFromGLTF(const char* fileName)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model    gltfModel;
    std::string        error, warning;

    if (!loader.LoadASCIIFromFile(&gltfModel, &error, &warning, fileName))
    {
        LOG("Model: Failed to load GLTF: %s", error.c_str());
        return false;
    }

    if (!warning.empty()) LOG("Model: GLTF Warning: %s", warning.c_str());

    std::string modelName = std::filesystem::path(fileName).stem().string();
    return SceneImporter::ImportFromLoadedGLTF(gltfModel, modelName);
}

bool Model::loadFromLibrary(const std::string& folder)
{
    ModuleFileSystem* fs = app->getFileSystem();

    if (!fs->Exists(folder.c_str()))
    {
        LOG("Model: Folder does not exist: %s", folder.c_str());
        return false;
    }

    char* buffer = nullptr;
    uint32_t fileSize = fs->Load((folder + "/scene.meta").c_str(), &buffer);

    if (!buffer || fileSize < sizeof(SceneImporter::SceneHeader))
    {
        delete[] buffer;
        LOG("Model: Failed to load scene metadata");
        return false;
    }

    SceneImporter::SceneHeader header;
    memcpy(&header, buffer, sizeof(header));
    delete[] buffer;

    if (header.magic != 0x53434E45 || header.version != 1)
    {
        LOG("Model: Invalid scene metadata");
        return false;
    }

    m_meshes.clear();
    m_materials.clear();

    for (uint32_t i = 0; i < header.meshCount; ++i)
    {
        std::unique_ptr<Mesh> mesh;
        std::string meshFile = folder + "/" + std::to_string(i) + ".mesh";
        if (MeshImporter::Load(meshFile, mesh))
            m_meshes.push_back(std::move(mesh));
        else
            LOG("  Failed to load mesh: %s", meshFile.c_str());
    }

    std::string materialFolder = fs->GetLibraryPath() + "Materials/" + std::filesystem::path(folder).filename().string();

    for (uint32_t i = 0; i < header.materialCount; ++i)
    {
        std::unique_ptr<Material> material;
        std::string matFile = materialFolder + "/" + std::to_string(i) + ".mat";
        if (MaterialImporter::Load(matFile, material))
            m_materials.push_back(std::move(material));
        else
        {
            LOG("  Failed to load material: %s", matFile.c_str());
            m_materials.push_back(std::make_unique<Material>());
        }
    }

    while (m_materials.size() < m_meshes.size())
        m_materials.push_back(std::make_unique<Material>());

    LOG("Model: Loaded %d meshes, %d materials from %s",
        (int)m_meshes.size(), (int)m_materials.size(), folder.c_str());

    return !m_meshes.empty();
}

void Model::draw(ID3D12GraphicsCommandList* cmdList, const Matrix& worldMatrix)
{
    Matrix finalWorld = (m_modelMatrix * worldMatrix).Transpose();
    cmdList->SetGraphicsRoot32BitConstants(1, 16, &finalWorld, 0);
    for (const auto& mesh : m_meshes)
        if (mesh) mesh->draw(cmdList);
}

void Model::draw(ID3D12GraphicsCommandList* cmdList)
{
    draw(cmdList, Matrix::Identity);
}