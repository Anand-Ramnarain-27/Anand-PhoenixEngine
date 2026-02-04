#pragma once

#include "ModuleShaderDescriptorsBase.h"

class ShaderTableDesc;

class ModuleShaderDescriptors : public ModuleShaderDescriptorsBase<
    4096,   
    8,  
    ShaderTableDesc>
{
public:
    // No preRender needed unless you add frame-based logic
    // void preRender() override {}

    // No createViewInternal needed - shader descriptors don't use it
    // void createViewInternal(ID3D12Resource* resource, const void* pDesc,
    //     D3D12_CPU_DESCRIPTOR_HANDLE destHandle) override {}

protected:
    void createViewInternal(ID3D12Resource* /*resource*/, const void* /*pDesc*/,
        D3D12_CPU_DESCRIPTOR_HANDLE /*destHandle*/) override
    {
      
    }
};