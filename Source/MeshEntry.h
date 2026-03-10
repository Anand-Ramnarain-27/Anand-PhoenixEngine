#pragma once

#include "ResourceCommon.h"
#include <wrl/client.h>
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

class Mesh;
class Material;

struct MeshEntry{
    UID meshUID = 0;
    UID materialUID = 0;
    class ResourceMesh* meshRes = nullptr;
    class ResourceMaterial* materialRes = nullptr;

    Mesh* mesh = nullptr;   
    Material* material = nullptr;  

    float worldMatrix[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    ComPtr<ID3D12Resource> materialCB;
};