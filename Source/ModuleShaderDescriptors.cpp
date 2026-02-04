#include "Globals.h"
#include "ModuleShaderDescriptors.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ShaderTableDesc.h" 

ModuleShaderDescriptors::ModuleShaderDescriptors()
{
    m_tables.resize(MAX_TABLES);
}

ModuleShaderDescriptors::~ModuleShaderDescriptors()
{
    cleanUp();
}

bool ModuleShaderDescriptors::init()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12) return false;

    m_device = d3d12->getDevice();

    m_descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = static_cast<UINT>(MAX_DESCRIPTORS);
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.NodeMask = 0;

    HRESULT hr = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap));
    if (FAILED(hr))
    {
        LOG("Failed to create shader-visible descriptor heap: 0x%08X", hr);
        return false;
    }

    for (size_t i = 0; i < MAX_TABLES; ++i)
    {
        m_tables[i].index = i;
        m_tables[i].isFree = true;
        m_tables[i].refCount = 0;
        m_tables[i].frameFreed = 0;

        size_t descriptorOffset = i * DESCRIPTORS_PER_TABLE;
        CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(
            m_descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            static_cast<INT>(descriptorOffset),
            static_cast<UINT>(m_descriptorSize)
        );
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(
            m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(),
            static_cast<INT>(descriptorOffset),
            static_cast<UINT>(m_descriptorSize)
        );

        m_tables[i].cpuHandle = cpuHandle;
        m_tables[i].gpuHandle = gpuHandle;

        m_freeTableIndices.push(i);
    }

    LOG("ModuleShaderDescriptors initialized with %zu tables (%zu descriptors total)",
        MAX_TABLES, MAX_DESCRIPTORS);

    return true;
}

bool ModuleShaderDescriptors::cleanUp()
{
    size_t stillUsed = 0;
    for (const auto& table : m_tables)
    {
        if (table.refCount > 0)
        {
            ++stillUsed;
        }
    }

    if (stillUsed > 0)
    {
        LOG("Warning: %zu descriptor tables still in use at cleanup", stillUsed);
    }

    m_descriptorHeap.Reset();
    m_device.Reset();

    while (!m_freeTableIndices.empty())
        m_freeTableIndices.pop();

    m_tables.clear();

    return true;
}

void ModuleShaderDescriptors::preRender()
{
    collectGarbage();
    ++m_currentFrame;
}

void ModuleShaderDescriptors::collectGarbage()
{
    const UINT SAFE_FRAME_DELAY = 3;

    for (auto& table : m_tables)
    {
        if (!table.isFree && table.refCount == 0 &&
            table.frameFreed > 0 &&
            m_currentFrame - table.frameFreed >= SAFE_FRAME_DELAY)
        {
            table.isFree = true;
            m_freeTableIndices.push(table.index);

            memset(table.name, 0, sizeof(table.name));
        }
    }
}

std::shared_ptr<ShaderTableDesc> ModuleShaderDescriptors::allocTable(const char* name)
{
    if (m_freeTableIndices.empty())
    {
        LOG("No free descriptor tables available!");
        return nullptr;
    }

    size_t index = m_freeTableIndices.front();
    m_freeTableIndices.pop();

    DescriptorTable& table = m_tables[index];
    table.isFree = false;
    table.refCount = 1;
    table.frameFreed = 0;

    if (name)
    {
        strncpy_s(table.name, name, sizeof(table.name) - 1);
    }
    else
    {
        snprintf(table.name, sizeof(table.name), "Table_%zu", index);
    }

    LOG("Allocated descriptor table %zu: %s", index, table.name);

    return std::shared_ptr<ShaderTableDesc>(new ShaderTableDesc(this, index));
}

void ModuleShaderDescriptors::freeTable(size_t index)
{
    if (index >= m_tables.size())
    {
        LOG("Invalid table index: %zu", index);
        return;
    }

    DescriptorTable& table = m_tables[index];
    if (table.isFree)
    {
        LOG("Table %zu is already free", index);
        return;
    }

    table.frameFreed = m_currentFrame;

    LOG("Marked descriptor table %zu for deferred freeing (frame %u)",
        index, m_currentFrame);
}

size_t ModuleShaderDescriptors::findFreeTable() const
{
    if (m_freeTableIndices.empty())
        return MAX_TABLES;

    return m_freeTableIndices.front();
}