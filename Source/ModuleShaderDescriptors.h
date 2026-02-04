#pragma once

#include "Module.h"
#include <array>

class ShaderTableDesc;

class ModuleShaderDescriptors : public Module
{
    friend class ShaderTableDesc;

public:
    ModuleShaderDescriptors();
    ~ModuleShaderDescriptors();

    bool init() override;
    void preRender() override;

    ID3D12DescriptorHeap* getHeap() { return heap.Get(); }

    ShaderTableDesc allocTable();

private:
    enum Constants {
        NUM_TABLES = 4096,
        DESCRIPTORS_PER_TABLE = 8,
        TOTAL_DESCRIPTORS = NUM_TABLES * DESCRIPTORS_PER_TABLE
    };

    UINT allocHandle();
    void freeHandle(UINT handle);
    void deferRelease(UINT handle);
    void collectGarbage();

    bool isValidHandle(UINT handle) const { return handle > 0 && handle <= freeList.size() && !freeList[handle - 1]; }
    UINT getDescriptorIndex(UINT handle) const { return (handle - 1) * DESCRIPTORS_PER_TABLE; }

    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(UINT handle, UINT slot) const {
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart, getDescriptorIndex(handle) + slot, descriptorSize);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(UINT handle, UINT slot) const {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, getDescriptorIndex(handle) + slot, descriptorSize);
    }

private:
    ComPtr<ID3D12DescriptorHeap> heap;

    // Handle management: 0 = free, 1 = allocated, >1 = deferred frame number
    std::array<UINT32, NUM_TABLES> freeList = { 0 };
    std::vector<UINT> freeStack;

    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = { 0 };
    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = { 0 };
    UINT descriptorSize = 0;
    UINT currentFrame = 0;
};