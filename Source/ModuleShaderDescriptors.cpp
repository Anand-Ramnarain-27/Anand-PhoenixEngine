#include "Globals.h"
#include "ModuleShaderDescriptors.h"
#include "ShaderTableDesc.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include <algorithm>

ModuleShaderDescriptors::ModuleShaderDescriptors()
{
    for (UINT i = 0; i < MAX_TABLES; ++i)
        m_freeHandles.push(i);
}

bool ModuleShaderDescriptors::init()
{
    auto d3d12 = app->getD3D12();
    if (!d3d12) return false;

    m_device = d3d12->getDevice();
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = static_cast<UINT>(MAX_TABLES * SLOTS_PER_TABLE);
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap))))
        return false;

    m_heap->SetName(L"ShaderDescriptorsHeap");
    m_descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
    m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();

    return true;
}

void ModuleShaderDescriptors::preRender()
{
    ++m_currentFrame;

    const UINT FRAME_DELAY = 3;
    for (UINT i = 0; i < MAX_TABLES; ++i)
    {
        auto& t = m_tables[i];
        if (t.refCount == 0 && t.frameFreed && m_currentFrame - t.frameFreed >= FRAME_DELAY)
        {
            t.name.clear();
            t.frameFreed = 0;
            m_freeHandles.push(i);
        }
    }
}

ShaderTableDesc ModuleShaderDescriptors::allocTable(const char* name)
{
    if (m_freeHandles.empty()) return ShaderTableDesc(); 

    UINT handle = m_freeHandles.front();
    m_freeHandles.pop();

    auto& t = m_tables[handle];
    t.refCount = 1;
    t.frameFreed = 0;
    t.name = name ? name : "Table_" + std::to_string(handle);

    return ShaderTableDesc(handle, &t.refCount, this);
}

void ModuleShaderDescriptors::releaseTable(UINT handle)
{
    if (handle >= MAX_TABLES) return;

    auto& t = m_tables[handle];
    if (--t.refCount == 0)
        t.frameFreed = m_currentFrame;
}

D3D12_GPU_DESCRIPTOR_HANDLE ModuleShaderDescriptors::getGPUHandle(UINT handle, UINT slot) const
{
    if (!isValidHandle(handle) || slot >= SLOTS_PER_TABLE) return {};
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_gpuStart, static_cast<INT>(handle * SLOTS_PER_TABLE + slot), m_descriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE ModuleShaderDescriptors::getCPUHandle(UINT handle, UINT slot) const
{
    if (!isValidHandle(handle) || slot >= SLOTS_PER_TABLE) return {};
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cpuStart, static_cast<INT>(handle * SLOTS_PER_TABLE + slot), m_descriptorSize);
}
