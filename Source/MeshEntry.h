#pragma once
#include "ResourceCommon.h"
#include "Material.h"
#include <SimpleMath.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>

using Microsoft::WRL::ComPtr;
using DirectX::SimpleMath::Vector2;
using DirectX::SimpleMath::Vector3;
using DirectX::SimpleMath::Vector4;

class Mesh;
class ResourceMesh;
class ResourceMaterial;

// Per-object VFX overrides (Phoenix::VFX mesh calls), applied on top of the material at draw time without touching
// the shared material: hit flashes, rims, fades, scrolling beams.
struct MeshVfxParams {
    Vector4 tint = Vector4(1.f, 1.f, 1.f, 1.f);   // multiplies base colour; alpha < 1 moves the mesh to the transparent forward pass
    Vector4 emissive = Vector4(0.f, 0.f, 0.f, 0.f); // rgb added as emissive; w = rim power (0 = flat over the whole surface)
    Vector2 uvOffset = Vector2(0.f, 0.f);
    Vector2 uvScroll = Vector2(0.f, 0.f);         // UV units per second on top of uvOffset (scaled game time)
    Vector4 baseColor = Vector4(0.f, 0.f, 0.f, 0.f); // w > 0: replaces the material's base colour (and alpha) before tint

    bool isDefault() const{
        return tint == Vector4(1.f, 1.f, 1.f, 1.f) && emissive == Vector4(0.f, 0.f, 0.f, 0.f) &&
               uvOffset == Vector2(0.f, 0.f) && uvScroll == Vector2(0.f, 0.f) && baseColor.w <= 0.f;
    }
};

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

    // Outside the game camera's frustum: not drawn to screen, but still a shadow caster, since an
    // off-screen object can cast onto visible ground.
    bool shadowOnly = false;

    // Copied from the owning ComponentMesh each frame by RuntimeCore (uvScroll already folded into uvOffset).
    MeshVfxParams vfx;

    // Filled by GBufferPass::render for the draws it issued this frame, so XRayPass can redraw the same
    // entries with the exact same per-draw CBs (identical depth) without its own upload rings.
    D3D12_GPU_VIRTUAL_ADDRESS gbMvpVA = 0;
    D3D12_GPU_VIRTUAL_ADDRESS gbInstVA = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE gbMatTable = {};
    bool gbDoubleSided = false;
};
