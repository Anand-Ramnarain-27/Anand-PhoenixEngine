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
class ModuleScene;
class GameObject;

// ---------------------------------------------------------------------------
enum class PrimitiveType {
    Cube, // unit cube, 6 faces with per-face normals
    Sphere, // UV sphere, configurable rings/segments
    Capsule, // hemisphere-cylinder-hemisphere, axis-aligned Y
    Plane, // flat XZ quad, normal +Y, good for floors
    Cylinder, // capped cylinder, axis-aligned Y
};

// ---------------------------------------------------------------------------
class PrimitiveFactory
{
public:
    // ---- Legacy textured-quad helpers (unchanged) ----
    static std::unique_ptr<Mesh> createQuadMesh();
    static std::unique_ptr<Model> createQuadModel(std::unique_ptr<Material> material);
    static std::unique_ptr<Model> createTexturedQuad(ComPtr<ID3D12Resource> texture,
                                                      D3D12_GPU_DESCRIPTOR_HANDLE srv);
    static GameObject* createTexturedQuadObject(ModuleScene* scene, const std::string& name,
                                                 ComPtr<ID3D12Resource> texture,
                                                 D3D12_GPU_DESCRIPTOR_HANDLE srv);

    // ---- Procedural primitive meshes ----
    static std::unique_ptr<Mesh> createCubeMesh();
    static std::unique_ptr<Mesh> createSphereMesh(int rings = 16, int segments = 16);
    static std::unique_ptr<Mesh> createCapsuleMesh(int halfRings = 8, int segments = 16);
    static std::unique_ptr<Mesh> createPlaneMesh();
    static std::unique_ptr<Mesh> createCylinderMesh(int segments = 16);

    // Convenience: wrap a mesh in a default-white-material Model.
    static std::unique_ptr<Model> meshToModel(std::unique_ptr<Mesh> mesh);
};
