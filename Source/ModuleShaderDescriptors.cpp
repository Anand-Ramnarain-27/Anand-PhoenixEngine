#include "Globals.h"
#include "ModuleShaderDescriptors.h"
#include "ShaderTableDesc.h"
#include "Application.h"
#include "ModuleD3D12.h"

ModuleShaderDescriptors::ModuleShaderDescriptors()
{
    for (UINT i = 0; i < MAX_TABLES; ++i)
    {
        m_freeBits.set(i);
        m_freeList.push_back(i);
    }
}

ModuleShaderDescriptors::~ModuleShaderDescriptors()
{
   
}

bool ModuleShaderDescriptors::init()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12) return false;

    m_device = d3d12->getDevice();

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = static_cast<UINT>(TOTAL_DESCRIPTORS);
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap));
    if (FAILED(hr)) return false;

    m_heap->SetName(L"ShaderDescriptorsHeap");

    m_descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
    m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();

    LOG("Descriptor heap initialized: %zu tables (%zu total descriptors)",
        MAX_TABLES, TOTAL_DESCRIPTORS);

    return true;
}

void ModuleShaderDescriptors::preRender()
{
    collectGarbage();
    ++m_currentFrame;
}

ShaderTableDesc ModuleShaderDescriptors::allocTable(const char* name)
{
    if (m_freeList.empty())
    {
        collectGarbage(); 
        if (m_freeList.empty())
        {
            LOG("No free descriptor tables!");
            return ShaderTableDesc();
        }
    }

    UINT handle = m_freeList.back();
    m_freeList.pop_back();
    m_freeBits.reset(handle); 

    if (name)
        strncpy_s(m_tables[handle].name, name, sizeof(m_tables[handle].name) - 1);
    else
        snprintf(m_tables[handle].name, sizeof(m_tables[handle].name), "Table_%u", handle);

    m_refCounts[handle] = 1;
    m_tables[handle].frameFreed = 0;

    LOG("Allocated descriptor table %u: %s", handle, m_tables[handle].name);

    return ShaderTableDesc(handle, &m_refCounts[handle], this);
}

void ModuleShaderDescriptors::freeHandle(UINT handle)
{
    if (!isValidHandle(handle)) return;

    m_tables[handle].frameFreed = m_currentFrame;
}

void ModuleShaderDescriptors::collectGarbage()
{
    const UINT FRAME_DELAY = 3;

    for (UINT i = 0; i < MAX_TABLES; ++i)
    {
        if (!m_freeBits.test(i) && 
            m_refCounts[i] == 0 && 
            m_tables[i].frameFreed > 0 && 
            m_currentFrame - m_tables[i].frameFreed >= FRAME_DELAY)
        {
            m_freeBits.set(i);
            m_freeList.push_back(i);
            m_tables[i].name[0] = '\0';
        }
    }
}

bool ModuleShaderDescriptors::isValidHandle(UINT handle) const
{
    return handle < MAX_TABLES && !m_freeBits.test(handle);
}

D3D12_GPU_DESCRIPTOR_HANDLE ModuleShaderDescriptors::getGPUHandle(UINT handle, UINT slot) const
{
    if (slot >= SLOTS_PER_TABLE) return D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };

    return CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_gpuStart,
        static_cast<INT>(handle * SLOTS_PER_TABLE + slot),
        static_cast<UINT>(m_descriptorSize)
    );
}

D3D12_CPU_DESCRIPTOR_HANDLE ModuleShaderDescriptors::getCPUHandle(UINT handle, UINT slot) const
{
    if (slot >= SLOTS_PER_TABLE) return D3D12_CPU_DESCRIPTOR_HANDLE{ 0 };

    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_cpuStart,
        static_cast<INT>(handle * SLOTS_PER_TABLE + slot),
        static_cast<UINT>(m_descriptorSize)
    );
}