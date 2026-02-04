#pragma once

#include "Module.h"
#include <vector>
#include <queue>
#include <memory>

class ModuleD3D12;
class ShaderTableDesc;

class ModuleShaderDescriptors : public Module
{
public:
    ModuleShaderDescriptors();
    ~ModuleShaderDescriptors() override;

    bool init() override;
    bool cleanUp() override;
    void preRender() override;

    std::shared_ptr<ShaderTableDesc> allocTable(const char* name = nullptr);

    ID3D12DescriptorHeap* getDescriptorHeap() const { return m_descriptorHeap.Get(); }

    void collectGarbage();

private:
    friend class ShaderTableDesc;

    struct DescriptorTable
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = { 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = { 0 };
        size_t index = 0;
        UINT refCount = 0;
        UINT frameFreed = 0;
        char name[64] = "";
        bool isFree = true;
    };

    void freeTable(size_t index);

    // Find a free table slot
    size_t findFreeTable() const;

private:
    static constexpr size_t MAX_TABLES = 4096;
    static constexpr size_t DESCRIPTORS_PER_TABLE = 8;
    static constexpr size_t MAX_DESCRIPTORS = MAX_TABLES * DESCRIPTORS_PER_TABLE;

    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;

    std::vector<DescriptorTable> m_tables;
    std::queue<size_t> m_freeTableIndices;

    size_t m_descriptorSize = 0;
    UINT m_currentFrame = 0;
};