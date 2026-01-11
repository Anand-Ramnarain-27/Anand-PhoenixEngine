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
    Model();
    ~Model();

    bool load(const char* fileName, const char* basePath);

    void draw(ID3D12GraphicsCommandList* cmdList);

    const Matrix& getModelMatrix() const { return m_modelMatrix; }
    void setModelMatrix(const Matrix& mat) { m_modelMatrix = mat; }

    const std::string& getSrcFile() const { return m_srcFile; }
    const std::vector<std::unique_ptr<Mesh>>& getMeshes() const { return m_meshes; }
    const std::vector<std::unique_ptr<Material>>& getMaterials() const { return m_materials; }

private:
    std::string m_srcFile;
    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::vector<std::unique_ptr<Material>> m_materials;

    Matrix m_modelMatrix = Matrix::Identity;

    Vector3 m_position;
    Vector3 m_rotation;
    Vector3 m_scale;
};
