#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <string>

class ModuleShaderDescriptors;

class ShaderTableDesc
{
public:
    ShaderTableDesc() = default;
    ShaderTableDesc(UINT handle, UINT* refCount, ModuleShaderDescriptors* mgr);
    ShaderTableDesc(const ShaderTableDesc& other);
    ShaderTableDesc(ShaderTableDesc&& other) noexcept;
    ~ShaderTableDesc();

    ShaderTableDesc& operator=(const ShaderTableDesc& other);
    ShaderTableDesc& operator=(ShaderTableDesc&& other) noexcept;

    explicit operator bool() const { return m_refCount && *m_refCount > 0 && m_manager; }

    UINT getHandle() const { return m_handle; }
    void reset() { release(); }
    bool isValid() const { return static_cast<bool>(*this); }

    void createCBV(ID3D12Resource* buffer, UINT slot = 0, UINT64 size = 0, UINT64 offset = 0);
    void createSRV(ID3D12Resource* resource, UINT slot = 0, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr);
    void createUAV(ID3D12Resource* resource, UINT slot = 0, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc = nullptr);

    void createBufferSRV(ID3D12Resource* buffer, UINT slot = 0,
        UINT firstElement = 0, UINT numElements = 0, UINT stride = 0);
    void createTexture2DSRV(ID3D12Resource* texture, UINT slot = 0,
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, UINT mipLevels = UINT_MAX);
    void createNullSRV(UINT slot = 0);

    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(UINT slot = 0) const;
    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(UINT slot = 0) const;

private:
    void addRef();
    void release();
    bool isValidSlot(UINT slot) const;

private:
    UINT m_handle = 0;
    UINT* m_refCount = nullptr;
    ModuleShaderDescriptors* m_manager = nullptr;
};
