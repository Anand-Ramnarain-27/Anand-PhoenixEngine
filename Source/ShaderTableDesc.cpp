#include "Globals.h"
#include "ShaderTableDesc.h"
#include "Application.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleD3D12.h"

bool ShaderTableDesc::validateSlot(UINT slot)
{
    return slot < ModuleShaderDescriptors::DESCRIPTORS_PER_TABLE;
}

D3D12_CPU_DESCRIPTOR_HANDLE ShaderTableDesc::getSlotCPUHandle(UINT slot) const
{
    if (!handle || !validateSlot(slot)) return { 0 };
    return app->getShaderDescriptors()->getCPUHandle(handle, slot);
}

void ShaderTableDesc::createCBV(ID3D12Resource* resource, UINT slot)
{
    if (!handle || !validateSlot(slot)) return;

    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptors->getCPUHandle(this->handle, slot);

    if (resource) {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = resource->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = static_cast<UINT>(resource->GetDesc().Width);
        app->getD3D12()->getDevice()->CreateConstantBufferView(&cbvDesc, handle);
    }
    else {
        app->getD3D12()->getDevice()->CreateConstantBufferView(nullptr, handle);
    }
}

void ShaderTableDesc::createSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, UINT slot)
{
    if (!handle || !validateSlot(slot)) return;

    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptors->getCPUHandle(this->handle, slot);
    app->getD3D12()->getDevice()->CreateShaderResourceView(resource, desc, handle);
}

void ShaderTableDesc::createUAV(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, UINT slot)
{
    if (!handle || !validateSlot(slot)) return;

    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptors->getCPUHandle(this->handle, slot);
    app->getD3D12()->getDevice()->CreateUnorderedAccessView(resource, nullptr, desc, handle);
}

void ShaderTableDesc::createBufferSRV(ID3D12Resource* resource, UINT firstElement, UINT numElements,
    UINT structureByteStride, UINT slot)
{
    if (!resource || !handle || !validateSlot(slot)) return;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = firstElement;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = structureByteStride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    createSRV(resource, &srvDesc, slot);
}

void ShaderTableDesc::createTexture2DSRV(ID3D12Resource* texture, DXGI_FORMAT format,
    UINT mipLevels, UINT mostDetailedMip, UINT slot)
{
    if (!texture || !handle || !validateSlot(slot)) return;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format == DXGI_FORMAT_UNKNOWN ? texture->GetDesc().Format : format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = mipLevels;
    srvDesc.Texture2D.MostDetailedMip = mostDetailedMip;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    createSRV(texture, &srvDesc, slot);
}

void ShaderTableDesc::createNullSRV(D3D12_SRV_DIMENSION dimension, DXGI_FORMAT format, UINT slot)
{
    if (!handle || !validateSlot(slot)) return;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = dimension;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    switch (dimension) {
    case D3D12_SRV_DIMENSION_TEXTURE2D:
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        break;
    case D3D12_SRV_DIMENSION_BUFFER:
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = 1;
        srvDesc.Buffer.StructureByteStride = 0;
        break;
    default:
        break;
    }

    createSRV(nullptr, &srvDesc, slot);
}

D3D12_GPU_DESCRIPTOR_HANDLE ShaderTableDesc::getGPUHandle(UINT slot) const
{
    if (!handle || !validateSlot(slot)) return { 0 };
    return app->getShaderDescriptors()->getGPUHandle(handle, slot);
}

D3D12_CPU_DESCRIPTOR_HANDLE ShaderTableDesc::getCPUHandle(UINT slot) const
{
    if (!handle || !validateSlot(slot)) return { 0 };
    return app->getShaderDescriptors()->getCPUHandle(handle, slot);
}