#include "Globals.h"
#include "Model.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleShaderDescriptors.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4018) 
#pragma warning(disable : 4267) 
#include "tiny_gltf.h"
#pragma warning(pop)

#include <imgui.h>

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

    // Load materials
    m_materials.reserve(gltfModel.materials.size());
    for (const auto& gltfMat : gltfModel.materials)
    {
        Material mat;
        if (mat.load(gltfMat, gltfModel, basePath))
        {
            m_materials.push_back(std::move(mat));
        }
    }

    // Ensure at least one material exists
    if (m_materials.empty())
    {
        m_materials.emplace_back();
    }

    // Load meshes
    size_t totalPrimitives = 0;
    for (const auto& gltfMesh : gltfModel.meshes)
        totalPrimitives += gltfMesh.primitives.size();

    m_meshes.reserve(totalPrimitives);
    for (const auto& gltfMesh : gltfModel.meshes)
    {
        for (const auto& prim : gltfMesh.primitives)
        {
            Mesh mesh;
            if (mesh.load(prim, gltfModel))
            {
                m_meshes.push_back(std::move(mesh));
            }
        }
    }

    LOG("Loaded model: %s (%zu meshes, %zu materials)",
        fileName, m_meshes.size(), m_materials.size());

    return !m_meshes.empty();
}

void Model::render(ID3D12GraphicsCommandList* cmdList) const
{
    if (!cmdList) return;

    // Bind model matrix (typically root constant or CBV)
    // This depends on your renderer architecture

    for (const auto& mesh : m_meshes)
    {
        int matID = mesh.getMaterialIndex();
        if (matID >= 0 && matID < int(m_materials.size()))
        {
            const auto& mat = m_materials[matID];
            if (mat.hasTexture())
            {
                cmdList->SetGraphicsRootDescriptorTable(3, mat.getGPUHandle());
            }
        }

        mesh.render(cmdList);
    }
}

void Model::update(float deltaTime)
{
    // Animation or transform updates would go here
}

void Model::showImGuiControls()
{
    ImGui::Text("Model: %s", m_srcFile.c_str());
    ImGui::Separator();
    ImGui::Text("Meshes: %zu", m_meshes.size());
    ImGui::Text("Materials: %zu", m_materials.size());
}