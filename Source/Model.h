#pragma once

#include "Mesh.h"
#include <vector>
#include <memory>
#include <string>

class Material;
class Model
{
public:
    bool load(const char* fileName);

    void draw(ID3D12GraphicsCommandList* cmdList, const Matrix& worldMatrix);

    // Keep the old one for backward compatibility
    void draw(ID3D12GraphicsCommandList* cmdList);

    void setModelMatrix(const Matrix& matrix) { m_modelMatrix = matrix; }
    const Matrix& getModelMatrix() const { return m_modelMatrix; }
    const std::string& getSrcFile() const { return m_srcFile; }
    const std::vector<std::unique_ptr<Mesh>>& getMeshes() const { return m_meshes; }
    const std::vector<std::unique_ptr<Material>>& getMaterials() const { return m_materials; }

private:
    bool importFromGLTF(const char* fileName);
    bool loadFromLibrary(const std::string& folder);

private:
    std::string m_srcFile;
    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::vector<std::unique_ptr<Material>> m_materials;
    Matrix m_modelMatrix = Matrix::Identity;
};