#pragma once
#include "ShaderTableDesc.h"
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
using Microsoft::WRL::ComPtr;

class ColorLUT {
public:
    bool loadCube(ID3D12Device* device, const std::string& path);
    bool isValid() const { return m_texture != nullptr && m_srv.isValid(); }

    D3D12_GPU_DESCRIPTOR_HANDLE getSrvHandle() const { return m_srv.getGPUHandle(0); }

private:
    bool createTexture3D(ID3D12Device* device, int size, const std::vector<float>& data);

    ComPtr<ID3D12Resource> m_texture;
    ShaderTableDesc m_srv;
    int m_size = 0;
};
