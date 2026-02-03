#pragma once

#include "ModuleDescriptorsBase.h"

template<size_t MaxTables, size_t DescriptorsPerTable, typename DescriptorType>
class ModuleShaderDescriptorsBase :
    public ModuleDescriptorsBase<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
    MaxTables* DescriptorsPerTable,
    DescriptorType,
    true>  
{
    using Base = ModuleDescriptorsBase<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        MaxTables* DescriptorsPerTable,
        DescriptorType,
        true>;

protected:
    using Base::handles;
    using Base::cpuStart;
    using Base::gpuStart;
    using Base::descriptorSize;

public:
    enum Constants {
        DESCRIPTORS_PER_TABLE = DescriptorsPerTable,
        NUM_TABLES = MaxTables,
        TOTAL_DESCRIPTORS = MaxTables * DescriptorsPerTable
    };

    bool init() override
    {
        bool result = Base::init();
        if (result)
        {
            this->heap->SetName(L"Shader Descriptors Heap");
        }
        return result;
    }

    DescriptorType createTable()
    {
        UINT handle = handles.allocHandle();
        if (handle == 0)
            return DescriptorType();

        UINT index = handles.indexFromHandle(handle);
        return DescriptorType(handle, &this->refCounts[index]);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(UINT handle, UINT slot) const
    {
        if (handle == 0 || slot >= DESCRIPTORS_PER_TABLE)
            return { 0 };

        UINT baseIndex = handles.indexFromHandle(handle);
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(
            gpuStart,
            baseIndex * DESCRIPTORS_PER_TABLE + slot,
            descriptorSize
        );
    }

    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(UINT handle, UINT slot) const
    {
        if (handle == 0 || slot >= DESCRIPTORS_PER_TABLE)
            return { 0 };

        UINT baseIndex = handles.indexFromHandle(handle);
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            cpuStart,
            baseIndex * DESCRIPTORS_PER_TABLE + slot,
            descriptorSize
        );
    }

    bool isValidSlot(UINT slot) const
    {
        return slot < DESCRIPTORS_PER_TABLE;
    }

    UINT getDescriptorIndex(UINT handle, UINT slot) const
    {
        return handles.indexFromHandle(handle) * DESCRIPTORS_PER_TABLE + slot;
    }
};