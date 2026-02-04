#pragma once
#include "Globals.h"
#include "Mesh.h"
#include "Material.h"
#include <vector>
#include <string>

class Model
{
public:
    Model() = default;
    ~Model() = default;

    // Enable move semantics
    Model(Model&&) = default;
    Model& operator=(Model&&) = default;

    // Disable copy
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    bool load(const char* fileName, const char* basePath = "");
    void render(ID3D12GraphicsCommandList* cmdList) const;

    const Matrix& getModelMatrix() const { return m_modelMatrix; }
    void setModelMatrix(const Matrix& mat) { m_modelMatrix = mat; }

    Matrix getNormalMatrix() const
    {
        Matrix normal = m_modelMatrix;
        normal.Translation(Vector3::Zero);
        normal.Invert();
        normal.Transpose();
        return normal;
    }

    const std::string& getSrcFile() const { return m_srcFile; }
    const std::vector<Mesh>& getMeshes() const { return m_meshes; }
    const std::vector<Material>& getMaterials() const { return m_materials; }

    size_t getMeshCount() const { return m_meshes.size(); }
    size_t getMaterialCount() const { return m_materials.size(); }

    void update(float deltaTime);
    void showImGuiControls();

private:
    std::string m_srcFile;
    std::vector<Mesh> m_meshes;
    std::vector<Material> m_materials;
    Matrix m_modelMatrix = Matrix::Identity;
};