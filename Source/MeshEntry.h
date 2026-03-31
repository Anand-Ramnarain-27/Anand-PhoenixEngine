#pragma once
#include "ResourceCommon.h"
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class Mesh;
class Material;
class ResourceMesh;
class ResourceMaterial;

struct MeshEntry {
    UID meshUID = 0;
    UID materialUID = 0;
    ResourceMesh* meshRes = nullptr;
    ResourceMaterial* materialRes = nullptr;
    Mesh* mesh = nullptr;
    Material* material = nullptr;
    float worldMatrix[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    ComPtr<ID3D12Resource> materialCB;

    D3D12_GPU_VIRTUAL_ADDRESS skinnedVertexVA = 0;
    bool useSkinnedVA = false;
};