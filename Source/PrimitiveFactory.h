#pragma once
#include "Mesh.h"
#include "Material.h"
#include "Model.h"
#include "ModuleD3D12.h"
#include <memory>
#include <string>

class ModuleScene;
class GameObject;

class PrimitiveFactory
{
public:
    static std::unique_ptr<Mesh>  createQuadMesh();
    static std::unique_ptr<Model> createQuadModel(std::unique_ptr<Material> material);
    static std::unique_ptr<Model> createTexturedQuad(ComPtr<ID3D12Resource> texture, D3D12_GPU_DESCRIPTOR_HANDLE srv);

    static GameObject* createTexturedQuadObject(ModuleScene* scene, const std::string& name, ComPtr<ID3D12Resource> texture, D3D12_GPU_DESCRIPTOR_HANDLE srv);
};