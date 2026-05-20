#pragma once
#include "ResourceCommon.h"
#include "Material.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>

using Microsoft::WRL::ComPtr;

class Mesh;
class ResourceMesh;
class ResourceMaterial;

struct MeshEntry {
    UID meshUID = 0;
    UID materialUID = 0;
    ResourceMesh* meshRes = nullptr;
    ResourceMaterial* materialRes = nullptr;
    Mesh* mesh = nullptr;
    Material* material = nullptr; // raw ptr for procedural/per-frame entries (Model::buildMeshEntries)
    // Per-instance deep copy for resource-backed entries; takes priority over materialRes.
    std::unique_ptr<Material> instanceMaterial;
    float worldMatrix[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    bool isSkinned = false;
    ComPtr<ID3D12Resource> materialCB;
};