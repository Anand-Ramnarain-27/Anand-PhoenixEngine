#pragma once
#include "Module.h"
#include <array>
#include <string>
#include <queue>
#include <cstdint>

class ShaderTableDesc;

class ModuleShaderDescriptors : public Module {
    friend class ShaderTableDesc;

public:
    static constexpr size_t MAX_TABLES = 16384;
    static constexpr size_t SLOTS_PER_TABLE = 8;

    ModuleShaderDescriptors();
    ~ModuleShaderDescriptors() override = default;

    bool init() override;
    void preRender() override;

    ShaderTableDesc allocTable(const char* name = nullptr);
    ID3D12DescriptorHeap* getHeap() const { return m_heap.Get(); }

    // Live descriptor-heap stats (in allocation units of one 8-slot table).
    size_t getUsedTables() const { return MAX_TABLES - m_freeHandles.size(); }
    size_t getTotalTables() const { return MAX_TABLES; }

private:
    struct Table {
        UINT refCount = 0;
        UINT frameFreed = 0;
        std::string name;
    };

    void releaseTable(UINT handle);
    bool isValidHandle(UINT handle) const { return handle < MAX_TABLES && m_tables[handle].refCount > 0; }

    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(UINT handle, UINT slot = 0) const;
    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(UINT handle, UINT slot = 0) const;

private:
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12DescriptorHeap> m_heap;

    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart = {};
    UINT m_descriptorSize = 0;
    UINT m_currentFrame = 0;

    std::array<Table, MAX_TABLES> m_tables;
    std::queue<UINT> m_freeHandles;
};
