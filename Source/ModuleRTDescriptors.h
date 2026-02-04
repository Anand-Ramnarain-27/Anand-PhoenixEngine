#pragma once

#include "ModuleDescriptorsBase.h"
#include "RenderTargetDesc.h"

class ModuleRTDescriptors : public ModuleDescriptorsBase<
    D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
    256,
    RenderTargetDesc>
{
public:
    RenderTargetDesc create(ID3D12Resource* resource)
    {
        return ModuleDescriptorsBase::create(resource, nullptr);
    }

    RenderTargetDesc create(ID3D12Resource* resource, UINT arraySlice, UINT mipSlice, DXGI_FORMAT format)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = format;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = mipSlice;
        rtvDesc.Texture2DArray.FirstArraySlice = arraySlice;
        rtvDesc.Texture2DArray.ArraySize = 1;
        rtvDesc.Texture2DArray.PlaneSlice = 0;

        return ModuleDescriptorsBase::create(resource, &rtvDesc);
    }

protected:
    void createViewInternal(ID3D12Resource* resource, const void* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE destHandle) override
    {
        app->getD3D12()->getDevice()->CreateRenderTargetView(
            resource,
            static_cast<const D3D12_RENDER_TARGET_VIEW_DESC*>(pDesc),
            destHandle
        );
    }
};