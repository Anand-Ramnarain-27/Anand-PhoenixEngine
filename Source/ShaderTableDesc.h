#pragma once

#include "Globals.h"
#include <memory>
#include <string>

class ModuleShaderDescriptors;

class ShaderTableDesc : public std::enable_shared_from_this<ShaderTableDesc>
{
public:
    ShaderTableDesc(ModuleShaderDescriptors* manager, size_t tableIndex);
    ~ShaderTableDesc();

    ShaderTableDesc(const ShaderTableDesc&) = delete;
    ShaderTableDesc& operator=(const ShaderTableDesc&) = delete;

    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle() const;
    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle() const;

    bool createCBV(UINT slot, ID3D12Resource* resource, const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc = nullptr);
    bool createSRV(UINT slot, ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr);
    bool createUAV(UINT slot, ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc = nullptr);

    bool createNullSRV(UINT slot, D3D12_SRV_DIMENSION dimension = D3D12_SRV_DIMENSION_TEXTURE2D);
    bool createNullUAV(UINT slot, D3D12_UAV_DIMENSION dimension = D3D12_UAV_DIMENSION_TEXTURE2D);

    bool createBufferSRV(UINT slot, ID3D12Resource* resource,
        UINT firstElement = 0, UINT numElements = 0,
        UINT structureByteStride = 0);
    bool createTexture2DSRV(UINT slot, ID3D12Resource* texture,
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
        UINT mipLevels = 1, UINT mostDetailedMip = 0);
    bool createTextureArraySRV(UINT slot, ID3D12Resource* textureArray,
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
        UINT arraySize = 1, UINT mipLevels = 1);
    bool createCubemapSRV(UINT slot, ID3D12Resource* cubemap,
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
        UINT mipLevels = 1);

    bool createConstantBufferView(UINT slot, ID3D12Resource* buffer,
        UINT64 sizeInBytes, UINT64 offset = 0);

    void addRef();
    void release();
    UINT getRefCount() const;

    size_t getIndex() const { return m_tableIndex; }
    const char* getName() const;
    bool isValid() const;

private:
    D3D12_CPU_DESCRIPTOR_HANDLE getSlotCPUHandle(UINT slot) const;

    bool isValidSlot(UINT slot) const;

private:
    ModuleShaderDescriptors* m_manager = nullptr;
    size_t m_tableIndex = 0;
    UINT m_refCount = 0;
    bool m_isValid = false;
};