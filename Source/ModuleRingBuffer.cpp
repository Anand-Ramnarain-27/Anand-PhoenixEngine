#include "Globals.h"
#include "ModuleRingBuffer.h"
#include "Application.h"
#include "ModuleD3D12.h"

static constexpr size_t DEFAULT_CAPACITY = 10 * 1024 * 1024;
static constexpr size_t MIN_ALLOCATION = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

bool ModuleRingBuffer::init()
{
    auto* d3d12 = app->getD3D12();
    auto* device = d3d12->getDevice();

    m_capacity = alignUp(DEFAULT_CAPACITY, 65536);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC   bufDesc = CD3DX12_RESOURCE_DESC::Buffer(m_capacity);

    if (FAILED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_buffer)))) return false;

    m_buffer->SetName(L"RingBuffer");

    CD3DX12_RANGE readRange(0, 0);
    m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedPtr));

    m_head = 0;
    m_tail = 0;
    m_currentFrame = d3d12->getCurrentBackBufferIdx();
    return true;
}

void ModuleRingBuffer::preRender()
{
    auto* d3d12 = app->getD3D12();
    uint32_t completedFrame = (d3d12->getCurrentBackBufferIdx() + 1) % MAX_FRAMES;
    m_currentFrame = d3d12->getCurrentBackBufferIdx();

    auto& completed = m_frameData[completedFrame];
    if (completed.allocationSize > 0)
    {
        m_tail = (completed.allocationStart + completed.allocationSize) % m_capacity;
        m_frameData[completedFrame] = {};
        if (m_tail == m_head) { m_head = 0; m_tail = 0; }
    }
}

D3D12_GPU_VIRTUAL_ADDRESS ModuleRingBuffer::allocateConstantBuffer(const void* data, size_t size)
{
    return allocateRaw(data, alignUp(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
}

D3D12_GPU_VIRTUAL_ADDRESS ModuleRingBuffer::allocateRaw(const void* data, size_t size)
{
    size = alignUp(size, MIN_ALLOCATION);

    if (size > m_capacity) { _ASSERT_EXPR(false, L"Allocation exceeds ring buffer capacity"); return 0; }

    size_t start = m_head;
    size_t end = start + size;

    if (end > m_capacity)
    {
        start = 0;
        end = size;
        if (end > m_tail && m_tail > m_head) { _ASSERT_EXPR(false, L"Ring buffer out of memory"); return 0; }
    }

    if (start < m_tail && end > m_tail) { _ASSERT_EXPR(false, L"Ring buffer out of memory"); return 0; }

    memcpy(m_mappedPtr + start, data, size);
    m_head = end;

    auto& frame = m_frameData[m_currentFrame];
    if (frame.allocationSize == 0) frame.allocationStart = start;
    frame.allocationSize += size;

    return m_buffer->GetGPUVirtualAddress() + start;
}

size_t ModuleRingBuffer::alignUp(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}