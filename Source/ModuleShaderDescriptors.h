#pragma once

#include "Module.h"
#include <array>
#include <vector>
#include <bitset>

class ShaderTableDesc;

class ModuleShaderDescriptors : public Module
{
    friend class ShaderTableDesc;

public:
    static constexpr size_t MAX_TABLES = 4096;
    static constexpr size_t SLOTS_PER_TABLE = 8;
    static constexpr size_t TOTAL_DESCRIPTORS = MAX_TABLES * SLOTS_PER_TABLE;

    ModuleShaderDescriptors();
    virtual ~ModuleShaderDescriptors();

    bool init() override;
    void preRender() override;

    ShaderTableDesc allocTable(const char* name = nullptr);
    ID3D12DescriptorHeap* getHeap() const { return m_heap.Get(); }

private:
    void freeHandle(UINT handle);
    void collectGarbage();

    bool isValidHandle(UINT handle) const;

    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(UINT handle, UINT slot = 0) const;
    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(UINT handle, UINT slot = 0) const;

private:
    struct TableEntry
    {
        UINT frameFreed = 0;
        char name[32] = "";
    };

    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12DescriptorHeap> m_heap;

    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart = { 0 };
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart = { 0 };
    UINT m_descriptorSize = 0;
    UINT m_currentFrame = 0;

    std::array<TableEntry, MAX_TABLES> m_tables;
    std::array<UINT, MAX_TABLES> m_refCounts = { 0 };
    std::bitset<MAX_TABLES> m_freeBits;  
    std::vector<UINT> m_freeList;
};