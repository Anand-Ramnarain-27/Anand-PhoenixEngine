#include "Globals.h"
#include "ShaderTableDesc.h"
#include "ModuleShaderDescriptors.h"
#include "Application.h"
#include "ModuleD3D12.h"

ShaderTableDesc::ShaderTableDesc(UINT handle, UINT* refCount, ModuleShaderDescriptors* mgr)
    : m_handle(handle), m_refCount(refCount), m_manager(mgr)
{
    addRef();
}

ShaderTableDesc::ShaderTableDesc(const ShaderTableDesc& other)
    : m_handle(other.m_handle), m_refCount(other.m_refCount), m_manager(other.m_manager)
{
    addRef();
}

ShaderTableDesc::ShaderTableDesc(ShaderTableDesc&& other) noexcept
    : m_handle(other.m_handle), m_refCount(other.m_refCount), m_manager(other.m_manager)
{
    other.m_handle = 0;
    other.m_refCount = nullptr;
    other.m_manager = nullptr;
}

ShaderTableDesc::~ShaderTableDesc()
{
    release();
}

ShaderTableDesc& ShaderTableDesc::operator=(const ShaderTableDesc& other)
{
    if (this != &other)
    {
        release();
        m_handle = other.m_handle;
        m_refCount = other.m_refCount;
        m_manager = other.m_manager;
        addRef();
    }
    return *this;
}

ShaderTableDesc& ShaderTableDesc::operator=(ShaderTableDesc&& other) noexcept
{
    if (this != &other)
    {
        release();
        m_handle = other.m_handle;
        m_refCount = other.m_refCount;
        m_manager = other.m_manager;
        other.m_handle = 0;
        other.m_refCount = nullptr;
        other.m_manager = nullptr;
    }
    return *this;
}

void ShaderTableDesc::addRef()
{
    if (m_refCount) ++(*m_refCount);
}

void ShaderTableDesc::release()
{
    if (!m_refCount) return;
    if (--(*m_refCount) == 0 && m_manager)
        m_manager->releaseTable(m_handle);
    m_handle = 0;
    m_refCount = nullptr;
    m_manager = nullptr;
}

bool ShaderTableDesc::isValidSlot(UINT slot) const
{
    return slot < m_descriptors.size();
}

D3D12_GPU_DESCRIPTOR_HANDLE ShaderTableDesc::getGPUHandle(UINT slot) const
{
    return m_manager ? m_manager->getGPUHandle(m_handle, slot) : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
}

D3D12_CPU_DESCRIPTOR_HANDLE ShaderTableDesc::getCPUHandle(UINT slot) const
{
    return m_manager ? m_manager->getCPUHandle(m_handle, slot) : D3D12_CPU_DESCRIPTOR_HANDLE{ 0 };
}

void ShaderTableDesc::createCBV(ID3D12Resource* buffer, UINT slot, UINT64 size, UINT64 offset)
{
    if (!isValid() || !isValidSlot(slot)) return;

    D3D12_CONSTANT_BUFFER_VIEW_DESC desc{};
    if (buffer)
    {
        desc.BufferLocation = buffer->GetGPUVirtualAddress() + offset;
        desc.SizeInBytes = size ? static_cast<UINT>(size) :
            static_cast<UINT>((buffer->GetDesc().Width + 255) & ~255);
    }

    auto device = app->getD3D12()->getDevice();
    device->CreateConstantBufferView(buffer ? &desc : nullptr, getCPUHandle(slot));
}

void ShaderTableDesc::createSRV(ID3D12Resource* resource, UINT slot, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc)
{
    if (!isValid() || !isValidSlot(slot)) return;
    app->getD3D12()->getDevice()->CreateShaderResourceView(resource, desc, getCPUHandle(slot));
}

void ShaderTableDesc::createUAV(ID3D12Resource* resource, UINT slot, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc)
{
    if (!isValid() || !isValidSlot(slot)) return;
    app->getD3D12()->getDevice()->CreateUnorderedAccessView(resource, nullptr, desc, getCPUHandle(slot));
}

void ShaderTableDesc::createBufferSRV(ID3D12Resource* buffer, UINT slot,
    UINT firstElement, UINT numElements, UINT stride)
{
    if (!buffer || !isValid() || !isValidSlot(slot)) return;

    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Buffer.FirstElement = firstElement;
    desc.Buffer.NumElements = numElements;
    desc.Buffer.StructureByteStride = stride;
    desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    createSRV(buffer, slot, &desc);
}

void ShaderTableDesc::createTexture2DSRV(ID3D12Resource* texture, UINT slot,
    DXGI_FORMAT format, UINT mipLevels)
{
    if (!texture || !isValid() || !isValidSlot(slot)) return;

    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = format == DXGI_FORMAT_UNKNOWN ? texture->GetDesc().Format : format;
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Texture2D.MostDetailedMip = 0;
    desc.Texture2D.MipLevels = mipLevels;
    desc.Texture2D.PlaneSlice = 0;
    desc.Texture2D.ResourceMinLODClamp = 0.0f;

    createSRV(texture, slot, &desc);
}

void ShaderTableDesc::createNullSRV(UINT slot)
{
    if (!isValid() || !isValidSlot(slot)) return;

    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Texture2D.MipLevels = 1;
    desc.Texture2D.MostDetailedMip = 0;
    desc.Texture2D.ResourceMinLODClamp = 0.0f;

    createSRV(nullptr, slot, &desc);
}
