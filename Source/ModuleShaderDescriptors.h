#pragma once

#include "ModuleShaderDescriptorsBase.h"
#include "ShaderTableDesc.h"

class ModuleShaderDescriptors : public ModuleShaderDescriptorsBase<
    4096,   
    8,     
    ShaderTableDesc>
{
public:
    void preRender() override
    {
        // Optional: Add frame-based garbage collection logic here if needed
        // The base class handles ref counting automatically
    }

protected:
    void createViewInternal(ID3D12Resource* resource, const void* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE destHandle) override
    {
        UNREFERENCED_PARAMETER(resource);
        UNREFERENCED_PARAMETER(pDesc);
        UNREFERENCED_PARAMETER(destHandle);
        assert(false && "Use createTable() for shader descriptors");
    }
};