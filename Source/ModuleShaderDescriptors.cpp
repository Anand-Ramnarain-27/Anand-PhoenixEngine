#include "Globals.h"
#include "ModuleShaderDescriptors.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ShaderTableDesc.h"

ModuleShaderDescriptors::ModuleShaderDescriptors()
{
    freeStack.reserve(NUM_TABLES);
    for (UINT i = 1; i <= NUM_TABLES; ++i) {
        freeStack.push_back(i);
    }
}

ModuleShaderDescriptors::~ModuleShaderDescriptors()
{
    assert(freeStack.size() == NUM_TABLES);
}

bool ModuleShaderDescriptors::init()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    ID3D12Device2* device = d3d12->getDevice();

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = TOTAL_DESCRIPTORS;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap)))) {
        return false;
    }

    heap->SetName(L"Shader Descriptors Heap");
    descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    gpuStart = heap->GetGPUDescriptorHandleForHeapStart();
    cpuStart = heap->GetCPUDescriptorHandleForHeapStart();

    return true;
}

void ModuleShaderDescriptors::preRender()
{
    collectGarbage();
    ++currentFrame;
}

UINT ModuleShaderDescriptors::allocHandle()
{
    if (freeStack.empty()) {
        return 0; 
    }

    UINT handle = freeStack.back();
    freeStack.pop_back();
    freeList[handle - 1] = 1; 
    return handle;
}

void ModuleShaderDescriptors::freeHandle(UINT handle)
{
    if (isValidHandle(handle)) {
        freeList[handle - 1] = 0;
        freeStack.push_back(handle);
    }
}

void ModuleShaderDescriptors::deferRelease(UINT handle)
{
    if (isValidHandle(handle)) {
        freeList[handle - 1] = currentFrame + 2;
    }
}

void ModuleShaderDescriptors::collectGarbage()
{
    const UINT FRAME_DELAY = 3;

    for (UINT i = 0; i < freeList.size(); ++i) {
        UINT state = freeList[i];
        if (state > 1 && currentFrame - (state - 2) >= FRAME_DELAY) {
            freeList[i] = 0;
            freeStack.push_back(i + 1);
        }
    }
}

ShaderTableDesc ModuleShaderDescriptors::allocTable()
{
    return ShaderTableDesc(allocHandle());
}