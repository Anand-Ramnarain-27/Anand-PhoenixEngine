#include "Globals.h"
#include "Model.h"
#include "Application.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE 
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#include <imgui.h>
#include <filesystem>

bool Model::load(const char* fileName, const char* basePath)
{
    return loadMaterialsAndMeshes(fileName, basePath, false);
}

bool Model::loadPBRPhong(const char* fileName, const char* basePath)
{
    return loadMaterialsAndMeshes(fileName, basePath, true);
}

bool Model::loadMaterialsAndMeshes(const char* fileName, const char* basePath, bool usePBR)
{
    namespace fs = std::filesystem;

    tinygltf::TinyGLTF loader;
    tinygltf::Model gltfModel;
    std::string error, warning;

    bool success = loader.LoadASCIIFromFile(&gltfModel, &error, &warning, fileName);

    if (!success)
    {
        LOG("Failed to load GLTF file: %s", error.c_str());
        return false;
    }

    if (!warning.empty())
    {
        LOG("GLTF warning: %s", warning.c_str());
    }

    m_srcFile = fileName;

    std::string actualBasePath;
    if (basePath)
    {
        actualBasePath = basePath;
    }
    else
    {
        fs::path filePath(fileName);
        actualBasePath = filePath.parent_path().string() + "/";
    }

    if (!loadMaterials(gltfModel, actualBasePath, usePBR))
    {
        LOG("Failed to load materials");
        return false;
    }

    if (!loadMeshes(gltfModel))
    {
        LOG("Failed to load meshes");
        return false;
    }

    LOG("Loaded model: %s (%zu meshes, %zu materials, PBR: %s)",
        fileName, m_meshes.size(), m_materials.size(), usePBR ? "yes" : "no");

    return true;
}

bool Model::loadMaterials(const tinygltf::Model& gltfModel, const std::string& basePath, bool usePBR)
{
    for (const auto& gltfMaterial : gltfModel.materials)
    {
        auto material = std::make_unique<Material>();
        bool loaded = false;

        if (usePBR)
        {
            loaded = material->loadPBRPhong(gltfMaterial, gltfModel, basePath.c_str());
        }
        else
        {
            loaded = material->load(gltfMaterial, gltfModel, basePath.c_str());
        }

        if (!loaded)
        {
            LOG("Warning: Failed to load material: %s", gltfMaterial.name.c_str());
        }
        m_materials.push_back(std::move(material));
    }

    if (m_materials.empty())
    {
        auto defaultMaterial = std::make_unique<Material>();
        m_materials.push_back(std::move(defaultMaterial));
        LOG("Created default material");
    }

    return true;
}

bool Model::loadMeshes(const tinygltf::Model& gltfModel)
{
    size_t totalMeshes = 0;
    for (const auto& gltfMesh : gltfModel.meshes)
    {
        totalMeshes += gltfMesh.primitives.size();
    }

    m_meshes.reserve(totalMeshes);

    for (const auto& gltfMesh : gltfModel.meshes)
    {
        for (const auto& primitive : gltfMesh.primitives)
        {
            auto mesh = std::make_unique<Mesh>();
            if (mesh->load(primitive, gltfModel))
            {
                m_meshes.push_back(std::move(mesh));
            }
            else
            {
                LOG("Warning: Failed to load mesh primitive");
            }
        }
    }

    return !m_meshes.empty();
}

void Model::draw(ID3D12GraphicsCommandList* commandList)
{
    for (const auto& mesh : m_meshes)
    {
        mesh->draw(commandList);
    }
}

size_t Model::getTotalVertexCount() const
{
    size_t total = 0;
    for (const auto& mesh : m_meshes)
    {
        total += mesh->getVertexCount();
    }
    return total;
}

size_t Model::getTotalTriangleCount() const
{
    size_t total = 0;
    for (const auto& mesh : m_meshes)
    {
        total += mesh->getIndexCount() / 3;
    }
    return total;
}

void Model::showImGuiControls()
{
    ImGui::Text("Model: %s", m_srcFile.c_str());
    ImGui::Separator();

    ImGui::Text("Transform");
    bool transformChanged = false;

    transformChanged |= ImGui::DragFloat3("Position", &m_position.x, 0.1f);
    transformChanged |= ImGui::DragFloat3("Rotation", &m_rotation.x, 0.01f);
    transformChanged |= ImGui::DragFloat3("Scale", &m_scale.x, 0.01f);

    if (transformChanged)
    {
        m_modelMatrix = Matrix::CreateScale(m_scale) *
            Matrix::CreateFromYawPitchRoll(m_rotation.y, m_rotation.x, m_rotation.z) *
            Matrix::CreateTranslation(m_position);
    }

    ImGui::Separator();
    ImGui::Text("Statistics:");
    ImGui::Text("Meshes: %zu", m_meshes.size());
    ImGui::Text("Materials: %zu", m_materials.size());
    ImGui::Text("Total Vertices: %zu", getTotalVertexCount());
    ImGui::Text("Total Triangles: %zu", getTotalTriangleCount());
}