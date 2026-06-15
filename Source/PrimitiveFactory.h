#pragma once
#include "Globals.h"
#include <memory>
#include <string>
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class Mesh;
class Material;
class Model;
class SceneGraph;
class GameObject;

enum class PrimitiveType {
    Cube,
    Sphere,
    Capsule,
    Plane,
    Cylinder,
};

class PrimitiveFactory {
public:
    static std::unique_ptr<Mesh> createQuadMesh();
    static std::unique_ptr<Model> createQuadModel(std::unique_ptr<Material> material);
    static std::unique_ptr<Model> createTexturedQuad(ComPtr<ID3D12Resource> texture,
                                                      D3D12_GPU_DESCRIPTOR_HANDLE srv);
    static GameObject* createTexturedQuadObject(SceneGraph* scene, const std::string& name,
                                                 ComPtr<ID3D12Resource> texture,
                                                 D3D12_GPU_DESCRIPTOR_HANDLE srv);

    static std::unique_ptr<Mesh> createCubeMesh();
    static std::unique_ptr<Mesh> createSphereMesh(int rings = 16, int segments = 16);
    static std::unique_ptr<Mesh> createCapsuleMesh(int halfRings = 8, int segments = 16);
    static std::unique_ptr<Mesh> createPlaneMesh();
    static std::unique_ptr<Mesh> createCylinderMesh(int segments = 16);

    static std::unique_ptr<Model> meshToModel(std::unique_ptr<Mesh> mesh);
};
