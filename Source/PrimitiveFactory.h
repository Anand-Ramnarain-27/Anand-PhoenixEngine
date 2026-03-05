#pragma once
#include <memory>
#include <string>
#include <d3d12.h>
#include <wrl/client.h>
#include <SimpleMath.h>

using Microsoft::WRL::ComPtr;

class Mesh;
class Material;
class Model;
class ModuleScene;
class GameObject;

class PrimitiveFactory
{
public:
    static std::unique_ptr<Mesh> createQuadMesh();
    static std::unique_ptr<Model> createQuadModel(std::unique_ptr<Material> material);
    static std::unique_ptr<Model> createTexturedQuad(ComPtr<ID3D12Resource> texture, D3D12_GPU_DESCRIPTOR_HANDLE srv);
    static GameObject* createTexturedQuadObject(ModuleScene* scene, const std::string& name, ComPtr<ID3D12Resource> texture, D3D12_GPU_DESCRIPTOR_HANDLE srv);
};