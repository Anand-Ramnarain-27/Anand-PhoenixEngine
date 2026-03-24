#pragma once

#include "Mesh.h"
#include "Material.h"
#include "MeshEntry.h"
#include <vector>
#include <memory>
#include <string>
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class ModuleStaticBuffer;

class Model {
public:
    bool load(const char* fileName, ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer);

    void buildMeshEntries(const Matrix& parentWorld, std::vector<MeshEntry>& out) const;
    void markMaterialsDirty() { m_materialCBsDirty = true; }

    void setModelMatrix(const Matrix& matrix) { m_modelMatrix = matrix; }
    const Matrix& getModelMatrix() const { return m_modelMatrix; }
    const std::string& getSrcFile() const { return m_srcFile; }

    const std::vector<std::unique_ptr<Mesh>>& getMeshes() const { return m_meshes; }
    const std::vector<std::unique_ptr<Material>>& getMaterials() const { return m_materials; }
    std::vector<std::unique_ptr<Material>>& getMaterialsMutable() { m_materialCBsDirty = true; return m_materials; }

    void addMesh(std::unique_ptr<Mesh> mesh) { m_meshes.push_back(std::move(mesh)); }
    void addMaterial(std::unique_ptr<Material> mat) { m_materials.push_back(std::move(mat)); m_materialCBsDirty = true; }

private:
    bool importFromGLTF(const char* fileName);
    bool loadFromLibrary(const std::string& folder, ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer);
    void rebuildMaterialCBs() const;

    std::string m_srcFile;
    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::vector<std::unique_ptr<Material>> m_materials;
    Matrix m_modelMatrix = Matrix::Identity;

    mutable std::vector<ComPtr<ID3D12Resource>> m_materialCBs;
    mutable bool m_materialCBsDirty = true;
};