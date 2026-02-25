#pragma once

#include <wrl.h>
#include <d3d12.h>
#include "ShaderTableDesc.h"

using Microsoft::WRL::ComPtr;

class EnvironmentMap
{
public:
    ComPtr<ID3D12Resource> texture;

    ShaderTableDesc srvTable;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};

    bool isValid() const
    {
        return texture != nullptr && srvTable.isValid();
    }
};