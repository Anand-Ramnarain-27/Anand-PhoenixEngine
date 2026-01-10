#include "Globals.h"
#include "ModuleRingBuffer.h"
#include "Application.h"
#include "ModuleD3D12.h"

ModuleRingBuffer::ModuleRingBuffer() = default;

ModuleRingBuffer::~ModuleRingBuffer()
{
    cleanUp();
}

bool ModuleRingBuffer::init()
{
    return createBuffer(DEFAULT_BUFFER_SIZE);
}

bool ModuleRingBuffer::cleanUp()
{
    if (m_buffer && m_mappedAddress)
    {
        D3D12_RANGE writtenRange = { 0, m_totalSize };
        m_buffer->Unmap(0, &writtenRange);
        m_mappedAddress = nullptr;
    }

    m_buffer.Reset();
    m_totalSize = 0;
    m_head = m_tail = m_usedMemory = 0;

    for (auto& frame : m_frameAllocations)
    {
        frame.clear();
    }

    return true;
}

void ModuleRingBuffer::preRender()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12) return;

    m_currentFrameIndex = d3d12->getCurrentBackBufferIdx();

    reclaimCompletedFrames();
}

bool ModuleRingBuffer::createBuffer(size_t size)
{
    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12) return false;

    size = alignUp(size, 65536);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

    HRESULT hr = d3d12->getDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_buffer)
    );

    if (FAILED(hr))
    {
        LOG("Failed to create ring buffer: 0x%08X", hr);
        return false;
    }

    D3D12_RANGE readRange = { 0, 0 };
    hr = m_buffer->Map(0, &readRange, &m_mappedAddress);
    if (FAILED(hr))
    {
        LOG("Failed to map ring buffer: 0x%08X", hr);
        return false;
    }

    m_gpuBaseAddress = m_buffer->GetGPUVirtualAddress();
    m_totalSize = size;
    m_head = m_tail = 0;
    m_usedMemory = 0;

    LOG("Ring buffer initialized: %.2f MB", size / (1024.0f * 1024.0f));
    return true;
}

RingBufferAllocation ModuleRingBuffer::allocate(size_t size, size_t alignment)
{
    RingBufferAllocation alloc;

    if (allocateInternal(size, alignment, alloc))
    {
        if (m_currentFrameIndex < FRAMES_IN_FLIGHT)
        {
            FrameAllocation frameAlloc;
            frameAlloc.startOffset = alloc.offset;
            frameAlloc.size = alloc.size;
            frameAlloc.frameIndex = app->getD3D12()->getCurrentFrame();
            m_frameAllocations[m_currentFrameIndex].push_back(frameAlloc);
        }
    }

    return alloc;
}

RingBufferAllocation ModuleRingBuffer::allocateForFrame(size_t size, size_t alignment)
{
    return allocate(size, alignment);
}

bool ModuleRingBuffer::allocateInternal(size_t size, size_t alignment, RingBufferAllocation& outAlloc)
{
    if (size == 0 || size > m_totalSize)
    {
        LOG("Invalid allocation size: %zu", size);
        return false;
    }

    size = alignUp(size, alignment);

    size_t freeSpace = 0;

    if (m_tail <= m_head)
    {
        freeSpace = m_totalSize - m_head;

        if (freeSpace >= size)
        {
            outAlloc.offset = m_head;
            outAlloc.size = size;
            m_head += size;
        }
        else
        {
            freeSpace += m_tail;

            if (freeSpace >= size)
            {
                outAlloc.offset = 0;
                outAlloc.size = size;
                m_head = size;
            }
            else
            {
                LOG("Ring buffer out of memory. Requested: %zu, Free: %zu", size, freeSpace);
                return false;
            }
        }
    }
    else
    {
        freeSpace = m_tail - m_head;

        if (freeSpace >= size)
        {
            outAlloc.offset = m_head;
            outAlloc.size = size;
            m_head += size;
        }
        else
        {
            LOG("Ring buffer out of memory. Requested: %zu, Free: %zu", size, freeSpace);
            return false;
        }
    }
    outAlloc.cpuAddress = static_cast<uint8_t*>(m_mappedAddress) + outAlloc.offset;
    outAlloc.gpuAddress = m_gpuBaseAddress + outAlloc.offset;

    m_usedMemory += size;

    return true;
}

void ModuleRingBuffer::reclaimCompletedFrames()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    if (!d3d12) return;

    unsigned lastCompletedFrame = d3d12->getLastCompletedFrame();

    for (unsigned frameIdx = 0; frameIdx < FRAMES_IN_FLIGHT; ++frameIdx)
    {
        auto& allocations = m_frameAllocations[frameIdx];

        while (!allocations.empty())
        {
            const FrameAllocation& alloc = allocations.front();

            if (alloc.frameIndex <= lastCompletedFrame)
            {
                m_tail = alloc.startOffset + alloc.size;
                m_usedMemory -= alloc.size;

                allocations.erase(allocations.begin());
            }
            else
            {
                break;
            }
        }
    }

    if (m_tail == m_head && m_usedMemory == 0)
    {
        m_tail = m_head = 0;
    }
}

void ModuleRingBuffer::reset()
{
    m_head = m_tail = 0;
    m_usedMemory = 0;

    for (auto& frame : m_frameAllocations)
    {
        frame.clear();
    }

    LOG("Ring buffer reset");
}