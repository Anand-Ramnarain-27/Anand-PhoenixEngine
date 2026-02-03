#pragma once

#include <array>
#include <memory>

#include "Globals.h"
#include "HandleManager.h"
#include "Module.h"
#include "Application.h"
#include "ModuleD3D12.h"

struct D3D12_RENDER_TARGET_VIEW_DESC;
struct D3D12_DEPTH_STENCIL_VIEW_DESC;

template<D3D12_DESCRIPTOR_HEAP_TYPE HeapType, size_t MaxDescriptors, typename DescriptorType>
class ModuleDescriptorsBase : public Module
{
    static_assert(MaxDescriptors > 0, "Must have at least one descriptor slot");

protected:
    HandleManager handles;

    ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = {};
    UINT descriptorSize = 0;

    std::array<UINT, MaxDescriptors> refCounts = {};

public:
    ModuleDescriptorsBase() : handles(MaxDescriptors) {}

    virtual ~ModuleDescriptorsBase()
    {
#ifdef _DEBUG
        _ASSERTE(handles.getFreeCount() == handles.getSize());
#endif
    }

    bool init() override
    {
        auto* device = app->getD3D12()->getDevice();
        descriptorSize = device->GetDescriptorHandleIncrementSize(HeapType);

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
            .Type = HeapType,
            .NumDescriptors = static_cast<UINT>(MaxDescriptors),
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0
        };

        HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap));
        if (SUCCEEDED(hr))
        {
            cpuStart = heap->GetCPUDescriptorHandleForHeapStart();
            return true;
        }
        return false;
    }

    DescriptorType createView(ID3D12Resource* resource, const void* pDesc = nullptr)
    {
        UINT handle = handles.allocHandle();
        if (handle == 0)
            return DescriptorType();

        UINT index = handles.indexFromHandle(handle);
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            cpuStart,
            index,
            descriptorSize
        );

        createViewInternal(resource, pDesc, cpuHandle);

        return DescriptorType(handle, &refCounts[index]);
    }

    void release(UINT handle)
    {
        if (handle != 0 && handles.validHandle(handle))
        {
            handles.freeHandle(handle);
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(UINT handle) const
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            cpuStart,
            handles.indexFromHandle(handle),
            descriptorSize
        );
    }

    bool isValid(UINT handle) const { return handles.validHandle(handle); }

protected:
    virtual void createViewInternal(ID3D12Resource* resource, const void* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE destHandle) = 0;
};