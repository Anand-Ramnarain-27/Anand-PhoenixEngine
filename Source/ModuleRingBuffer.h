#pragma once

#include "Module.h"
#include <cstdint>

class ModuleRingBuffer : public Module
{
public:
    ModuleRingBuffer() = default;
    ~ModuleRingBuffer() = default;

    bool init()      override;
    void preRender() override;

    template<typename T>
    D3D12_GPU_VIRTUAL_ADDRESS allocate(const T& data) { return allocateRaw(&data, sizeof(T)); }

    template<typename T>
    D3D12_GPU_VIRTUAL_ADDRESS allocate(const T* data, size_t count = 1) { return allocateRaw(data, sizeof(T) * count); }

    D3D12_GPU_VIRTUAL_ADDRESS allocateConstantBuffer(const void* data, size_t size);

private:
    D3D12_GPU_VIRTUAL_ADDRESS allocateRaw(const void* data, size_t size);
    static size_t alignUp(size_t size, size_t alignment);

    ComPtr<ID3D12Resource> m_buffer;
    uint8_t* m_mappedPtr = nullptr;
    size_t                 m_capacity = 0;
    size_t                 m_head = 0;
    size_t                 m_tail = 0;

    static constexpr uint32_t MAX_FRAMES = 3;
    struct FrameData { size_t allocationStart = 0; size_t allocationSize = 0; };
    FrameData  m_frameData[MAX_FRAMES] = {};
    uint32_t   m_currentFrame = 0;
};