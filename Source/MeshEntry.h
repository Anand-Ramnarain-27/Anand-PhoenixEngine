#pragma once
#include "ResourceBase.h"
#include <wrl/client.h>
#include <d3d12.h>
using Microsoft::WRL::ComPtr;

struct MeshEntry
{
    UID meshUID = 0;
    UID materialUID = 0;

    class ResourceMesh* meshRes = nullptr;
    class ResourceMaterial* materialRes = nullptr;

    ComPtr<ID3D12Resource> materialCB;
};