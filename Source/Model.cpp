#include "Globals.h"
#include "Model.h"
#include "Application.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE 
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#include <imgui.h>

Model::Model()
    : m_position(0, 0, 0)
    , m_rotation(0, 0, 0)
    , m_scale(1, 1, 1)
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
    tinygltf::Model model;
    std::string error, warning;

    bool success = loader.LoadASCIIFromFile(&model, &error, &warning, fileName);

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

    for (const auto& gltfMaterial : model.materials)
    {
        auto material = std::make_unique<Material>();
        material->load(gltfMaterial, model, basePath);
        m_materials.push_back(std::move(material));
    }

    if (m_materials.empty())
    {
        auto defaultMaterial = std::make_unique<Material>();
        m_materials.push_back(std::move(defaultMaterial));
    }

    for (const auto& gltfMesh : model.meshes)
    {
        for (const auto& primitive : gltfMesh.primitives)
        {
            auto mesh = std::make_unique<Mesh>();
            mesh->load(primitive, model);
            m_meshes.push_back(std::move(mesh));
        }
    }

    LOG("Loaded model: %s (%zu meshes, %zu materials)",
        fileName, m_meshes.size(), m_materials.size());

    return true;
}

void Model::draw(ID3D12GraphicsCommandList* commandList)
{
    for (const auto& mesh : m_meshes)
    {
        mesh->draw(commandList);
    }
}

void Model::showImGuiControls()
{
    ImGui::Text("Model: %s", m_srcFile.c_str());
    ImGui::Separator();

    ImGui::Text("Transform");
    if (ImGui::DragFloat3("Position", &m_position.x, 0.1f))
    {
        m_modelMatrix = Matrix::CreateScale(m_scale) *
            Matrix::CreateFromYawPitchRoll(m_rotation.y, m_rotation.x, m_rotation.z) *
            Matrix::CreateTranslation(m_position);
    }
    if (ImGui::DragFloat3("Rotation", &m_rotation.x, 0.01f))
    {
        m_modelMatrix = Matrix::CreateScale(m_scale) *
            Matrix::CreateFromYawPitchRoll(m_rotation.y, m_rotation.x, m_rotation.z) *
            Matrix::CreateTranslation(m_position);
    }
    if (ImGui::DragFloat3("Scale", &m_scale.x, 0.01f))
    {
        m_modelMatrix = Matrix::CreateScale(m_scale) *
            Matrix::CreateFromYawPitchRoll(m_rotation.y, m_rotation.x, m_rotation.z) *
            Matrix::CreateTranslation(m_position);
    }

    ImGui::Separator();
    ImGui::Text("Meshes: %zu", m_meshes.size());
    ImGui::Text("Materials: %zu", m_materials.size());
}