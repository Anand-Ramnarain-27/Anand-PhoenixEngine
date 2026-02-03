#pragma once

#include "DescriptorBase.h"

class ModuleShaderDescriptors;

class ShaderTableDesc : public DescriptorBase<ShaderTableDesc, ModuleShaderDescriptors>
{
    using Base = DescriptorBase<ShaderTableDesc, ModuleShaderDescriptors>;
    friend Base;

public:
    using Base::Base;

    void createCBV(ID3D12Resource* resource, UINT slot = 0);
    void createSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr, UINT slot = 0);
    void createUAV(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc = nullptr, UINT slot = 0);

    void createBufferSRV(ID3D12Resource* resource, UINT firstElement = 0, UINT numElements = 0,
        UINT structureByteStride = 0, UINT slot = 0);
    void createTexture2DSRV(ID3D12Resource* texture, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
        UINT mipLevels = 1, UINT mostDetailedMip = 0, UINT slot = 0);
    void createNullSRV(D3D12_SRV_DIMENSION dimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, UINT slot = 0);

    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(UINT slot = 0) const;
    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(UINT slot = 0) const;

    static ModuleShaderDescriptors* getModule();

private:
    static bool validateSlot(UINT slot);
};