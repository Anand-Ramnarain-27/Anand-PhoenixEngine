#pragma once
#include "ResourceCommon.h"
#include "Material.h"
#include <SimpleMath.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>

using Microsoft::WRL::ComPtr;
using DirectX::SimpleMath::Vector3;

class Mesh;
class ResourceMesh;
class ResourceMaterial;

struct MeshEntry {
    UID meshUID = 0;
    UID materialUID = 0;
    ResourceMesh* meshRes = nullptr;
    ResourceMaterial* materialRes = nullptr;
    Mesh* mesh = nullptr;
    Material* material = nullptr;
    std::unique_ptr<Material> instanceMaterial;
    float worldMatrix[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    bool isSkinned = false;
    D3D12_GPU_VIRTUAL_ADDRESS skinnedVA = 0;
    ComPtr<ID3D12Resource> materialCB;

    Vector3 aabbMin = {};
    Vector3 aabbMax = {};
    bool hasWorldAABB = false;

    // X-ray: 0 = none, 1..N = 1 + index into EditorSceneSettings::xray.tags (inherited from tagged ancestors).
    uint8_t xrayGroup = 0;

    // Filled by GBufferPass::render for the draws it issued this frame, so XRayPass can redraw the same
    // entries with the exact same per-draw CBs (identical depth) without its own upload rings.
    D3D12_GPU_VIRTUAL_ADDRESS gbMvpVA = 0;
    D3D12_GPU_VIRTUAL_ADDRESS gbInstVA = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE gbMatTable = {};
    bool gbDoubleSided = false;
};
