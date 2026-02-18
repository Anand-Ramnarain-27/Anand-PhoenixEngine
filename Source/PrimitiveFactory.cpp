#include "Globals.h"
#include "PrimitiveFactory.h"
#include "Material.h"
#include "ComponentMesh.h"
#include "ModuleScene.h"
#include "GameObject.h"

std::unique_ptr<Mesh> PrimitiveFactory::createQuadMesh()
{
    std::vector<Mesh::Vertex> verts =
    {
        { Vector3(-0.5f, -0.5f, 0.0f), Vector2(0.0f, 1.0f), Vector3(0, 0, 1) },
        { Vector3(-0.5f,  0.5f, 0.0f), Vector2(0.0f, 0.0f), Vector3(0, 0, 1) },
        { Vector3(0.5f,  0.5f, 0.0f), Vector2(1.0f, 0.0f), Vector3(0, 0, 1) },
        { Vector3(0.5f, -0.5f, 0.0f), Vector2(1.0f, 1.0f), Vector3(0, 0, 1) },
    };

    std::vector<uint32_t> indices = { 0, 1, 2,  0, 2, 3 };

    auto mesh = std::make_unique<Mesh>();
    mesh->setData(verts, indices, 0);
    return mesh;
}

std::unique_ptr<Model> PrimitiveFactory::createQuadModel(std::unique_ptr<Material> material)
{
    auto model = std::make_unique<Model>();
    model->addMesh(createQuadMesh());
    model->addMaterial(std::move(material));
    return model;
}

std::unique_ptr<Model> PrimitiveFactory::createTexturedQuad(
    ComPtr<ID3D12Resource> texture,
    D3D12_GPU_DESCRIPTOR_HANDLE srv)
{
    auto material = std::make_unique<Material>();
    material->setBaseColorTexture(texture, srv);
    return createQuadModel(std::move(material));
}

GameObject* PrimitiveFactory::createTexturedQuadObject(ModuleScene* scene, const std::string& name, ComPtr<ID3D12Resource> texture, D3D12_GPU_DESCRIPTOR_HANDLE srv)
{
    GameObject* go = scene->createGameObject(name);
    ComponentMesh* mc = go->createComponent<ComponentMesh>();
    mc->setModel(createTexturedQuad(texture, srv));
    return go;
}