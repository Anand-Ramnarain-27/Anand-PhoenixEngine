#pragma once

#include "Module.h"

#include <filesystem>
#include <vector>

namespace DirectX { class ScratchImage;  struct TexMetadata; }

class ModuleResources : public Module
{
private:

    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;

    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts;
    std::vector<UINT> numRows;
    std::vector<UINT64> rowSizes;

    struct DeferredFree
    {
        UINT frame = 0;
        ComPtr<ID3D12Resource> resource;
    };

    std::vector<DeferredFree> deferredFrees;

public:

    ModuleResources();
    ~ModuleResources();

    bool init() override;
    void preRender() override;
    bool cleanUp() override;

    ComPtr<ID3D12Resource> createUploadBuffer(const void* data, size_t size, const char* name);
    ComPtr<ID3D12Resource> createDefaultBuffer(const void* data, size_t size, const char* name);

    ComPtr<ID3D12Resource> createTextureFromFile(const std::filesystem::path& path, bool defaultSRGB = false);

    ComPtr<ID3D12Resource> createRenderTarget(DXGI_FORMAT format, size_t width, size_t height, UINT sampleCount, const Vector4& clearColour, const char* name);
    ComPtr<ID3D12Resource> createDepthStencil(DXGI_FORMAT format, size_t width, size_t height, UINT sampleCount, float clearDepth, uint8_t clearStencil, const char* name);

    void deferRelease(ComPtr<ID3D12Resource> resource);

private:

    ComPtr<ID3D12Resource> createTextureFromImage(const ScratchImage& image, const char* name);
    ComPtr<ID3D12Resource> createRenderTarget(DXGI_FORMAT format, size_t width, size_t height, size_t arraySize, size_t mipLevels, UINT sampleCount, const Vector4& clearColour, const char* name);
    ComPtr<ID3D12Resource> getUploadHeap(size_t size);
};

inline ComPtr<ID3D12Resource> ModuleResources::createRenderTarget(DXGI_FORMAT format, size_t width, size_t height, UINT sampleCount, const Vector4& clearColour, const char* name)
{
    return createRenderTarget(format, width, height, 1, 1, sampleCount, clearColour, name);
}