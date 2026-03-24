#include "Globals.h"
#include "Model.h"
#include "Mesh.h"
#include "Material.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleFileSystem.h"
#include "ModuleStaticBuffer.h"
#include "SceneImporter.h"
#include "MeshImporter.h"
#include "MaterialImporter.h"
#include "ImporterUtils.h"
#include "tiny_gltf.h"
#include <filesystem>
#include <cstring>
#include <d3dx12.h>

static bool materialsNeedReimport(const std::string& matFolder, uint32_t materialCount) {
    if (materialCount == 0) return false;

    std::string firstMat = ImporterUtils::IndexedPath(matFolder, 0, ".mat");
    if (!app->getFileSystem()->Exists(firstMat.c_str())) return true;

    MaterialImporter::MaterialHeader header;
    std::vector<char> buf;
    if (!ImporterUtils::LoadBuffer(firstMat, header, buf)) return true;
    if (!ImporterUtils::ValidateHeader(header, 0x4D415452)) return true;

    return header.version < 4;
}

bool Model::load(const char* fileName, ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer) {
    m_srcFile = fileName;
    std::string modelName = std::filesystem::path(fileName).stem().string();
    std::string meshFolder = app->getFileSystem()->GetLibraryPath() + "Meshes/" + modelName;
    std::string matFolder = app->getFileSystem()->GetLibraryPath() + "Materials/" + modelName;

    bool meshFolderExists = app->getFileSystem()->Exists(meshFolder.c_str());

    if (!meshFolderExists) {
        LOG("Model: importing %s", fileName);
        if (!importFromGLTF(fileName)) { LOG("Model: Failed to import %s", fileName); return false; }
    }
    else {
        SceneImporter::SceneHeader sceneHeader;
        if (SceneImporter::LoadSceneMetadata(modelName, sceneHeader)) {
            if (materialsNeedReimport(matFolder, sceneHeader.materialCount)) {
                LOG("Model: Material cache outdated, re-importing %s", fileName);
                importFromGLTF(fileName);
            }
        }
    }

    return loadFromLibrary(meshFolder, cmd, staticBuffer);
}

bool Model::importFromGLTF(const char* fileName) {
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

bool Model::loadFromLibrary(const std::string& folder, ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer) {
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
        if (MeshImporter::Load(meshFile, cmd, staticBuffer, mesh)) m_meshes.push_back(std::move(mesh));
        else LOG("Failed to load mesh: %s", meshFile.c_str());
    }

    std::string matFolder = fs->GetLibraryPath() + "Materials/" + std::filesystem::path(folder).filename().string();
    for (uint32_t i = 0; i < header.materialCount; ++i) {
        std::unique_ptr<Material> material;
        std::string matFile = ImporterUtils::IndexedPath(matFolder, i, ".mat");
        if (MaterialImporter::Load(matFile, material)) m_materials.push_back(std::move(material));
        else { LOG("Failed to load material: %s", matFile.c_str()); m_materials.push_back(std::make_unique<Material>()); }
    }

    while (m_materials.size() < m_meshes.size()) m_materials.push_back(std::make_unique<Material>());

    LOG("Model: Loaded %d meshes, %d materials from %s", (int)m_meshes.size(), (int)m_materials.size(), folder.c_str());
    return !m_meshes.empty();
}

void Model::rebuildMaterialCBs() const {
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

void Model::buildMeshEntries(const Matrix& parentWorld, std::vector<MeshEntry>& out) const {
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