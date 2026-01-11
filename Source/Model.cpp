#include "Globals.h"
#include "Model.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#include <imgui.h>

Model::Model()
    : m_position(0, 0, 0), m_rotation(0, 0, 0), m_scale(1, 1, 1)
{
    m_modelMatrix = Matrix::Identity;
}

Model::~Model()
{
    m_meshes.clear();
    m_materials.clear();
}

bool Model::load(const char* fileName, const char* basePath)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model gltfModel;
    std::string err, warn;

    bool success = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, fileName);
    if (!success)
    {
        LOG("Failed to load GLTF: %s", err.c_str());
        return false;
    }
    if (!warn.empty())
        LOG("GLTF warning: %s", warn.c_str());

    m_srcFile = fileName;

    for (const auto& gltfMat : gltfModel.materials)
    {
        auto mat = std::make_unique<Material>();
        mat->load(gltfMat, gltfModel, basePath);
        m_materials.push_back(std::move(mat));
    }

    if (m_materials.empty())
        m_materials.push_back(std::make_unique<Material>()); 

    for (const auto& gltfMesh : gltfModel.meshes)
    {
        for (const auto& prim : gltfMesh.primitives)
        {
            auto mesh = std::make_unique<Mesh>();
            mesh->load(prim, gltfModel);
            m_meshes.push_back(std::move(mesh));
        }
    }

    LOG("Loaded model: %s (%zu meshes, %zu materials)", fileName, m_meshes.size(), m_materials.size());
    return true;
}

void Model::draw(ID3D12GraphicsCommandList* cmdList)
{
    if (!cmdList) return;

    for (const auto& meshPtr : m_meshes)
    {
        Mesh* mesh = meshPtr.get();
        Material* mat = nullptr;

        int matID = mesh->getMaterialIndex();
        if (matID >= 0 && matID < (int)m_materials.size())
            mat = m_materials[matID].get();

        if (mat && mat->hasTexture())
            cmdList->SetGraphicsRootDescriptorTable(3, mat->getTextureGPUHandle());

        mesh->render(cmdList);
    }
}


