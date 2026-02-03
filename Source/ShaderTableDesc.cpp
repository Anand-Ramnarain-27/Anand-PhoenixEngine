#include "Globals.h"
#include "ShaderTableDesc.h"
#include "ModuleShaderDescriptors.h"
#include "Application.h"
#include "ModuleD3D12.h" 

ShaderTableDesc::ShaderTableDesc(ModuleShaderDescriptors* manager, size_t tableIndex)
    : m_manager(manager)
    , m_tableIndex(tableIndex)
    , m_refCount(1)
    , m_isValid(true)
{
}

ShaderTableDesc::~ShaderTableDesc()
{
    if (m_isValid && m_manager)
    {
        m_manager->freeTable(m_tableIndex);
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE ShaderTableDesc::getGPUHandle() const
{
    if (!m_isValid || !m_manager)
        return { 0 };

    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12)
        return { 0 };

    ID3D12Device* device = d3d12->getDevice();
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    CD3DX12_GPU_DESCRIPTOR_HANDLE handle(
        m_manager->getDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(),
        static_cast<INT>(m_tableIndex * 8),
        static_cast<UINT>(descriptorSize)
    );

    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE ShaderTableDesc::getCPUHandle() const
{
    if (!m_isValid || !m_manager)
        return { 0 };

    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12)
        return { 0 };

    ID3D12Device* device = d3d12->getDevice();
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
        m_manager->getDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(),
        static_cast<INT>(m_tableIndex * 8),
        static_cast<UINT>(descriptorSize)
    );

    return handle;
}

bool ShaderTableDesc::isValidSlot(UINT slot) const
{
    return slot < 8;
}

D3D12_CPU_DESCRIPTOR_HANDLE ShaderTableDesc::getSlotCPUHandle(UINT slot) const
{
    if (!isValidSlot(slot))
        return { 0 };

    D3D12_CPU_DESCRIPTOR_HANDLE baseHandle = getCPUHandle();
    if (baseHandle.ptr == 0)
        return { 0 };

    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12)
        return { 0 };

    ID3D12Device* device = d3d12->getDevice();
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE slotHandle = {
        baseHandle.ptr + slot * descriptorSize
    };

    return slotHandle;
}

bool ShaderTableDesc::createCBV(UINT slot, ID3D12Resource* resource, const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc)
{
    if (!isValidSlot(slot) || !m_isValid || !resource)
        return false;

    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12)
        return false;

    ID3D12Device* device = d3d12->getDevice();
    D3D12_CPU_DESCRIPTOR_HANDLE handle = getSlotCPUHandle(slot);

    if (desc)
    {
        device->CreateConstantBufferView(desc, handle);
    }
    else
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = resource->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = static_cast<UINT>(alignUp(resource->GetDesc().Width, 256));

        device->CreateConstantBufferView(&cbvDesc, handle);
    }

    return true;
}

bool ShaderTableDesc::createSRV(UINT slot, ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc)
{
    if (!isValidSlot(slot) || !m_isValid || !resource)
        return false;

    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12)
        return false;

    ID3D12Device* device = d3d12->getDevice();
    D3D12_CPU_DESCRIPTOR_HANDLE handle = getSlotCPUHandle(slot);

    if (desc)
    {
        device->CreateShaderResourceView(resource, desc, handle);
    }
    else
    {
        device->CreateShaderResourceView(resource, nullptr, handle);
    }

    return true;
}

bool ShaderTableDesc::createUAV(UINT slot, ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc)
{
    if (!isValidSlot(slot) || !m_isValid || !resource)
        return false;

    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12)
        return false;

    ID3D12Device* device = d3d12->getDevice();
    D3D12_CPU_DESCRIPTOR_HANDLE handle = getSlotCPUHandle(slot);

    if (desc)
    {
        device->CreateUnorderedAccessView(resource, nullptr, desc, handle);
    }
    else
    {
        device->CreateUnorderedAccessView(resource, nullptr, nullptr, handle);
    }

    return true;
}

bool ShaderTableDesc::createNullSRV(UINT slot, D3D12_SRV_DIMENSION dimension)
{
    if (!isValidSlot(slot) || !m_isValid)
        return false;

    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12)
        return false;

    ID3D12Device* device = d3d12->getDevice();
    D3D12_CPU_DESCRIPTOR_HANDLE handle = getSlotCPUHandle(slot);

    D3D12_SHADER_RESOURCE_VIEW_DESC nullDesc = {};
    nullDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    nullDesc.ViewDimension = dimension;
    nullDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    switch (dimension)
    {
    case D3D12_SRV_DIMENSION_TEXTURE2D:
        nullDesc.Texture2D.MipLevels = 1;
        nullDesc.Texture2D.MostDetailedMip = 0;
        nullDesc.Texture2D.PlaneSlice = 0;
        nullDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        break;

    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
        nullDesc.Texture2DArray.MipLevels = 1;
        nullDesc.Texture2DArray.MostDetailedMip = 0;
        nullDesc.Texture2DArray.FirstArraySlice = 0;
        nullDesc.Texture2DArray.ArraySize = 1;
        nullDesc.Texture2DArray.PlaneSlice = 0;
        nullDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
        break;

    case D3D12_SRV_DIMENSION_TEXTURECUBE:
        nullDesc.TextureCube.MipLevels = 1;
        nullDesc.TextureCube.MostDetailedMip = 0;
        nullDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        break;

    case D3D12_SRV_DIMENSION_BUFFER:
        nullDesc.Buffer.FirstElement = 0;
        nullDesc.Buffer.NumElements = 1;
        nullDesc.Buffer.StructureByteStride = 16;
        nullDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        break;

    default:
        break;
    }

    device->CreateShaderResourceView(nullptr, &nullDesc, handle);
    return true;
}

bool ShaderTableDesc::createNullUAV(UINT slot, D3D12_UAV_DIMENSION dimension)
{
    if (!isValidSlot(slot) || !m_isValid)
        return false;

    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12)
        return false;

    ID3D12Device* device = d3d12->getDevice();
    D3D12_CPU_DESCRIPTOR_HANDLE handle = getSlotCPUHandle(slot);

    D3D12_UNORDERED_ACCESS_VIEW_DESC nullDesc = {};
    nullDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    nullDesc.ViewDimension = dimension;

    device->CreateUnorderedAccessView(nullptr, nullptr, &nullDesc, handle);
    return true;
}

bool ShaderTableDesc::createBufferSRV(UINT slot, ID3D12Resource* resource,
    UINT firstElement, UINT numElements,
    UINT structureByteStride)
{
    if (!isValidSlot(slot) || !m_isValid || !resource)
        return false;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    srvDesc.Buffer.FirstElement = firstElement;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = structureByteStride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    return createSRV(slot, resource, &srvDesc);
}

bool ShaderTableDesc::createTexture2DSRV(UINT slot, ID3D12Resource* texture,
    DXGI_FORMAT format, UINT mipLevels,
    UINT mostDetailedMip)
{
    if (!isValidSlot(slot) || !m_isValid || !texture)
        return false;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format == DXGI_FORMAT_UNKNOWN ? texture->GetDesc().Format : format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    srvDesc.Texture2D.MipLevels = mipLevels;
    srvDesc.Texture2D.MostDetailedMip = mostDetailedMip;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    return createSRV(slot, texture, &srvDesc);
}

bool ShaderTableDesc::createTextureArraySRV(UINT slot, ID3D12Resource* textureArray,
    DXGI_FORMAT format, UINT arraySize,
    UINT mipLevels)
{
    if (!isValidSlot(slot) || !m_isValid || !textureArray)
        return false;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format == DXGI_FORMAT_UNKNOWN ? textureArray->GetDesc().Format : format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    srvDesc.Texture2DArray.MipLevels = mipLevels;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = arraySize;
    srvDesc.Texture2DArray.PlaneSlice = 0;
    srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

    return createSRV(slot, textureArray, &srvDesc);
}

bool ShaderTableDesc::createCubemapSRV(UINT slot, ID3D12Resource* cubemap,
    DXGI_FORMAT format, UINT mipLevels)
{
    if (!isValidSlot(slot) || !m_isValid || !cubemap)
        return false;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format == DXGI_FORMAT_UNKNOWN ? cubemap->GetDesc().Format : format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    srvDesc.TextureCube.MipLevels = mipLevels;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

    return createSRV(slot, cubemap, &srvDesc);
}

bool ShaderTableDesc::createConstantBufferView(UINT slot, ID3D12Resource* buffer,
    UINT64 sizeInBytes, UINT64 offset)
{
    if (!isValidSlot(slot) || !m_isValid || !buffer)
        return false;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = buffer->GetGPUVirtualAddress() + offset;
    cbvDesc.SizeInBytes = static_cast<UINT>(alignUp(sizeInBytes, 256));

    return createCBV(slot, buffer, &cbvDesc);
}

void ShaderTableDesc::addRef()
{
    ++m_refCount;
}

void ShaderTableDesc::release()
{
    if (m_refCount > 0)
    {
        --m_refCount;
        if (m_refCount == 0)
        {
            m_isValid = false;
            if (m_manager)
            {
                m_manager->freeTable(m_tableIndex);
            }
        }
    }
}

UINT ShaderTableDesc::getRefCount() const
{
    return m_refCount;
}

const char* ShaderTableDesc::getName() const
{
    if (!m_isValid || !m_manager)
        return "Invalid";

    return "ShaderTableDesc";
}

bool ShaderTableDesc::isValid() const
{
    return m_isValid && m_manager != nullptr;
}