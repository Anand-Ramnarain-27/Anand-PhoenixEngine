#pragma once

#include "Module.h"
#include <cstdint>

class ModuleRingBuffer : public Module
{
public:
    ModuleRingBuffer() = default;
    ~ModuleRingBuffer() = default;

    bool init() override;
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
    size_t m_capacity = 0;
    size_t m_head = 0;
    size_t m_tail = 0;

    static constexpr uint32_t MAX_FRAMES = 3;
    struct FrameData { size_t allocationStart = 0; size_t allocationSize = 0; };
    FrameData m_frameData[MAX_FRAMES] = {};
    uint32_t m_currentFrame = 0;

public:
    // Stats accessors for GPU Memory panel — declared after private members so
    // MAX_FRAMES and m_* are visible to the initialiser and inline bodies.
    static constexpr uint32_t kFrameCount = MAX_FRAMES;

    float getUsedMB() const {
        size_t used = (m_head >= m_tail) ? (m_head - m_tail) : (m_head + (m_capacity - m_tail));
        return float(used) / (1024.f * 1024.f);
    }
    float getTotalMB() const { return float(m_capacity) / (1024.f * 1024.f); }
    float getFrameUsedMB(uint32_t frameIdx) const {
        if (frameIdx >= MAX_FRAMES) return 0.f;
        return float(m_frameData[frameIdx].allocationSize) / (1024.f * 1024.f);
    }
    size_t   getHeadOffsetBytes() const { return m_head; }
    uint32_t getCurrentFrameIdx() const { return m_currentFrame; }
};
