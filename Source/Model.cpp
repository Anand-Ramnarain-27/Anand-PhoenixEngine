#include "Globals.h"
#include "Model.h"
#include "Mesh.h"
#include "Material.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleFileSystem.h"
#include "SceneImporter.h"
#include "MeshImporter.h"
#include "MaterialImporter.h"
#include "ImporterUtils.h"
#include "tiny_gltf.h"
#include <filesystem>
#include <cstring>
#include <d3dx12.h>

// Returns true if the cached material files need to be re-imported.
// Peeks at the first .mat file's version header without fully loading it.
// Triggers re-import when the version is older than the current format (v3+).
static bool materialsNeedReimport(const std::string& matFolder, uint32_t materialCount)
{
    if (materialCount == 0) return false;

    std::string firstMat = ImporterUtils::IndexedPath(matFolder, 0, ".mat");
    if (!app->getFileSystem()->Exists(firstMat.c_str())) return true;

    MaterialImporter::MaterialHeader header;
    std::vector<char> buf;
    if (!ImporterUtils::LoadBuffer(firstMat, header, buf)) return true;
    if (!ImporterUtils::ValidateHeader(header, 0x4D415452)) return true;

    // Version 3 added the metallic-roughness texture path.
    // Anything older must be re-imported.
    return header.version < 3;
}

bool Model::load(const char* fileName)
{
    m_srcFile = fileName;
    std::string modelName = std::filesystem::path(fileName).stem().string();
    std::string meshFolder = app->getFileSystem()->GetLibraryPath() + "Meshes/" + modelName;
    std::string matFolder = app->getFileSystem()->GetLibraryPath() + "Materials/" + modelName;

    bool meshFolderExists = app->getFileSystem()->Exists(meshFolder.c_str());

    if (!meshFolderExists)
    {
        LOG("Model: Scene not imported yet, importing %s", fileName);
        if (!importFromGLTF(fileName)) { LOG("Model: Failed to import scene from %s", fileName); return false; }
    }
    else
    {
        // Mesh folder exists - check if material files are up to date.
        // Load the scene header just to know how many materials there are.
        SceneImporter::SceneHeader sceneHeader;
        if (SceneImporter::LoadSceneMetadata(modelName, sceneHeader))
        {
            if (materialsNeedReimport(matFolder, sceneHeader.materialCount))
            {
                LOG("Model: Material cache is outdated (pre-v3), re-importing %s", fileName);
                if (!importFromGLTF(fileName))
                {
                    LOG("Model: Re-import failed for %s - continuing with stale materials", fileName);
                    // Don't return false - still try to load what we have
                }
            }
        }
    }

    return loadFromLibrary(meshFolder);
}

bool Model::importFromGLTF(const char* fileName)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model gltfModel;
    std::string error, warning;
    if (!loader.LoadASCIIFromFile(&gltfModel, &error, &warning, fileName)) {
        LOG("Model: Failed to load GLTF: %s", error.c_str());
        return false;
    }
    if (!warning.empty()) LOG("Model: GLTF Warning: %s", warning.c_str());
    return SceneImporter::ImportFromLoadedGLTF(gltfModel, std::filesystem::path(fileName).stem().string());
}

bool Model::loadFromLibrary(const std::string& folder)
{
    ModuleFileSystem* fs = app->getFileSystem();
    if (!fs->Exists(folder.c_str())) { LOG("Model: Folder does not exist: %s", folder.c_str()); return false; }

    SceneImporter::SceneHeader header;
    if (!SceneImporter::LoadSceneMetadata(std::filesystem::path(folder).filename().string(), header)) {
        LOG("Model: Failed to load scene metadata");
        return false;
    }

    m_meshes.clear();
    m_materials.clear();

    for (uint32_t i = 0; i < header.meshCount; ++i) {
        std::unique_ptr<Mesh> mesh;
        std::string meshFile = ImporterUtils::IndexedPath(folder, i, ".mesh");
        if (MeshImporter::Load(meshFile, mesh)) m_meshes.push_back(std::move(mesh));
        else LOG("  Failed to load mesh: %s", meshFile.c_str());
    }

    std::string matFolder = fs->GetLibraryPath() + "Materials/" + std::filesystem::path(folder).filename().string();
    for (uint32_t i = 0; i < header.materialCount; ++i) {
        std::unique_ptr<Material> material;
        std::string matFile = ImporterUtils::IndexedPath(matFolder, i, ".mat");
        if (MaterialImporter::Load(matFile, material)) m_materials.push_back(std::move(material));
        else { LOG("  Failed to load material: %s", matFile.c_str()); m_materials.push_back(std::make_unique<Material>()); }
    }

    while (m_materials.size() < m_meshes.size()) m_materials.push_back(std::make_unique<Material>());

    LOG("Model: Loaded %d meshes, %d materials from %s", (int)m_meshes.size(), (int)m_materials.size(), folder.c_str());
    return !m_meshes.empty();
}

void Model::draw(ID3D12GraphicsCommandList* cmdList, const Matrix& worldMatrix)
{
    Matrix finalWorld = m_modelMatrix * worldMatrix;
    cmdList->SetGraphicsRoot32BitConstants(1, 16, &finalWorld, 0);
    for (const auto& mesh : m_meshes) if (mesh) mesh->draw(cmdList);
}

void Model::draw(ID3D12GraphicsCommandList* cmdList) { draw(cmdList, Matrix::Identity); }

void Model::rebuildMaterialCBs() const
{
    m_materialCBs.resize(m_materials.size());
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(Material::Data) + 255) & ~255);

    for (size_t i = 0; i < m_materials.size(); ++i) {
        Material::Data data = {};
        if (m_materials[i]) data = m_materials[i]->getData();
        m_materialCBs[i].Reset();
        app->getD3D12()->getDevice()->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_materialCBs[i]));
        if (m_materialCBs[i]) {
            void* mapped = nullptr;
            m_materialCBs[i]->Map(0, nullptr, &mapped);
            memcpy(mapped, &data, sizeof(data));
            m_materialCBs[i]->Unmap(0, nullptr);
            m_materialCBs[i]->SetName(L"ModelMaterialCB");
        }
    }
    m_materialCBsDirty = false;
}

void Model::buildMeshEntries(const Matrix& parentWorld, std::vector<MeshEntry>& out) const
{
    if (m_materialCBsDirty || m_materialCBs.size() != m_materials.size()) rebuildMaterialCBs();

    Matrix finalWorld = m_modelMatrix * parentWorld;
    for (size_t i = 0; i < m_meshes.size(); ++i) {
        Mesh* mesh = m_meshes[i].get();
        if (!mesh) continue;
        MeshEntry entry;
        entry.mesh = mesh;
        entry.material = (i < m_materials.size()) ? m_materials[i].get() : nullptr;
        entry.materialCB = (i < m_materialCBs.size()) ? m_materialCBs[i] : nullptr;
        static_assert(sizeof(finalWorld) == sizeof(entry.worldMatrix), "Matrix size mismatch");
        memcpy(entry.worldMatrix, &finalWorld, sizeof(entry.worldMatrix));
        out.push_back(std::move(entry));
    }
}