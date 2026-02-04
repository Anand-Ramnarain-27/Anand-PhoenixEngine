#pragma once

#include "ModuleDescriptorsBase.h"
#include "DepthStencilDesc.h"

class ModuleDSDescriptors : public ModuleDescriptorsBase<
    D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
    256,
    DepthStencilDesc>
{
public:
    DepthStencilDesc create(ID3D12Resource* resource)
    {
        return ModuleDescriptorsBase::create(resource, nullptr);
    }

protected:
    void createViewInternal(ID3D12Resource* resource, const void* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE destHandle) override
    {
        app->getD3D12()->getDevice()->CreateDepthStencilView(
            resource,
            static_cast<const D3D12_DEPTH_STENCIL_VIEW_DESC*>(pDesc),
            destHandle
        );
    }
};