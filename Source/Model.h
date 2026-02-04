#pragma once

#include "Globals.h"
#include "Mesh.h"
#include "Material.h"
#include <vector>
#include <memory>
#include <string>

class Model
{
public:
    Model() = default;
    ~Model() = default;

    bool load(const char* fileName, const char* basePath = nullptr);

    bool loadPBRPhong(const char* fileName, const char* basePath = nullptr);

    void draw(ID3D12GraphicsCommandList* commandList);

    const Matrix& getModelMatrix() const { return m_modelMatrix; }
    void setModelMatrix(const Matrix& matrix) { m_modelMatrix = matrix; }

    const std::string& getSrcFile() const { return m_srcFile; }
    const std::vector<std::unique_ptr<Mesh>>& getMeshes() const { return m_meshes; }
    const std::vector<std::unique_ptr<Material>>& getMaterials() const { return m_materials; }

    size_t getMeshCount() const { return m_meshes.size(); }
    size_t getMaterialCount() const { return m_materials.size(); }
    size_t getTotalVertexCount() const;
    size_t getTotalTriangleCount() const;

    void showImGuiControls();

private:
    bool loadMaterials(const tinygltf::Model& gltfModel, const std::string& basePath, bool usePBR = false);
    bool loadMeshes(const tinygltf::Model& gltfModel);

    bool loadMaterialsAndMeshes(const char* fileName, const char* basePath, bool usePBR);

    std::string m_srcFile;
    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::vector<std::unique_ptr<Material>> m_materials;
    Matrix m_modelMatrix = Matrix::Identity;

    Vector3 m_position = Vector3::Zero;
    Vector3 m_rotation = Vector3::Zero;
    Vector3 m_scale = Vector3::One;
};