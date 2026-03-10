#pragma once

#include <d3d12.h>
#include <d3dx12.h>
#include <wrl.h>
#include <string>

using Microsoft::WRL::ComPtr;

class ModuleStaticBuffer
{
public:
    bool init(ID3D12Device* device, size_t poolSizeBytes = 512ull * 1024 * 1024);

    D3D12_VERTEX_BUFFER_VIEW allocVertexBuffer(ID3D12GraphicsCommandList* cmd, const void* srcData, size_t sizeBytes, UINT strideBytes, const std::string& debugName = "");
    D3D12_INDEX_BUFFER_VIEW allocIndexBuffer(ID3D12GraphicsCommandList* cmd, const void* srcData, size_t sizeBytes, DXGI_FORMAT format = DXGI_FORMAT_R32_UINT, const std::string& debugName = "");
    D3D12_GPU_VIRTUAL_ADDRESS allocConstantBuffer(size_t sizeBytes, void** outCpuPtr, const std::string& debugName = "");

    void reset();
    void shutdown();

    size_t getUsedBytes()  const { return m_defaultOffset; }
    size_t getTotalBytes() const { return m_poolSize; }
    float  getUsagePercent() const{
        return m_poolSize > 0
            ? 100.f * float(m_defaultOffset) / float(m_poolSize)
            : 0.f;
    }

private:
    static constexpr size_t kAlignment = 256; 

    size_t alignUp(size_t value) const{
        return (value + kAlignment - 1) & ~(kAlignment - 1);
    }
    size_t suballocate(size_t sizeBytes);

    size_t suballocateCB(size_t sizeBytes);

    ComPtr<ID3D12Resource> m_defaultHeapBuffer;

    ComPtr<ID3D12Resource> m_uploadHeapBuffer;

    uint8_t* m_uploadPtr = nullptr; 

    size_t m_poolSize = 0;
    size_t m_defaultOffset = 0;
    size_t m_cbOffset = 0; 

    bool m_initialized = false;
};