#pragma once

#include "Globals.h"
#include "Module.h"
#include <functional>
#include <vector>

struct RingBufferAllocation
{
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
    void* cpuAddress = nullptr;
    size_t size = 0;
    size_t offset = 0;
    bool isValid() const { return gpuAddress != 0 && cpuAddress != nullptr && size > 0; }
};

constexpr size_t CONSTANT_BUFFER_ALIGNMENT = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

class ModuleRingBuffer : public Module
{
public:
    ModuleRingBuffer();
    ~ModuleRingBuffer() override;

    bool init() override;
    bool cleanUp() override;
    void preRender() override;

    RingBufferAllocation allocate(size_t size, size_t alignment = CONSTANT_BUFFER_ALIGNMENT);

    template<typename T>
    RingBufferAllocation allocateType(const T& data, size_t alignment = CONSTANT_BUFFER_ALIGNMENT)
    {
        RingBufferAllocation alloc = allocate(sizeof(T), alignment);
        if (alloc.isValid()) {
            memcpy(alloc.cpuAddress, &data, sizeof(T));
        }
        return alloc;
    }

    template<typename T>
    RingBufferAllocation allocateArray(const T* data, size_t count, size_t alignment = CONSTANT_BUFFER_ALIGNMENT)
    {
        RingBufferAllocation alloc = allocate(sizeof(T) * count, alignment);
        if (alloc.isValid() && data) {
            memcpy(alloc.cpuAddress, data, sizeof(T) * count);
        }
        return alloc;
    }

    template<typename T>
    RingBufferAllocation allocateDynamic(const std::function<void(void*)>& fillFunc, size_t alignment = CONSTANT_BUFFER_ALIGNMENT)
    {
        RingBufferAllocation alloc = allocate(sizeof(T), alignment);
        if (alloc.isValid() && fillFunc) {
            fillFunc(alloc.cpuAddress);
        }
        return alloc;
    }

    RingBufferAllocation allocateForFrame(size_t size, size_t alignment = CONSTANT_BUFFER_ALIGNMENT);

    size_t getTotalSize() const { return m_totalSize; }
    size_t getUsedMemory() const { return m_usedMemory; }
    size_t getFreeMemory() const { return m_totalSize - m_usedMemory; }
    float getUsagePercentage() const {
        return m_totalSize > 0 ? (float)m_usedMemory / m_totalSize * 100.0f : 0.0f;
    }

    D3D12_GPU_VIRTUAL_ADDRESS getBaseGPUAddress() const { return m_gpuBaseAddress; }
    void* getBaseCPUAddress() const { return m_mappedAddress; }

    void reset();

private:
    bool createBuffer(size_t size);
    bool allocateInternal(size_t size, size_t alignment, RingBufferAllocation& outAlloc);
    void reclaimCompletedFrames();

    struct FrameAllocation
    {
        size_t startOffset = 0;
        size_t size = 0;
        unsigned frameIndex = 0;
    };

    ComPtr<ID3D12Resource> m_buffer;
    void* m_mappedAddress = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuBaseAddress = 0;

    size_t m_totalSize = 0;
    size_t m_head = 0;
    size_t m_tail = 0;
    size_t m_usedMemory = 0;

    std::vector<FrameAllocation> m_frameAllocations[FRAMES_IN_FLIGHT];
    unsigned m_currentFrameIndex = 0;

    static constexpr size_t DEFAULT_BUFFER_SIZE = 16 * 1024 * 1024;
};