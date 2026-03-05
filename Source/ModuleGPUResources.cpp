#include "Globals.h"
#include "ModuleGPUResources.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "DirectXTex.h"

using namespace DirectX;

ModuleGPUResources::ModuleGPUResources() = default;
ModuleGPUResources::~ModuleGPUResources() = default;

std::wstring ModuleGPUResources::toWString(const char* name){
    return std::wstring(name, name + strlen(name));
}

bool ModuleGPUResources::init(){
    ModuleD3D12* d3d12 = app->getD3D12();
    ID3D12Device4* device = d3d12->getDevice();
    bool ok = SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
    ok = ok && SUCCEEDED(device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&commandList)));
    ok = ok && SUCCEEDED(commandList->Reset(commandAllocator.Get(), nullptr));
    return ok;
}

bool ModuleGPUResources::cleanUp() { return true; }

void ModuleGPUResources::executeAndFlush(){
    ModuleD3D12* d3d12 = app->getD3D12();
    commandList->Close();
    ID3D12CommandList* lists[] = { commandList.Get() };
    d3d12->getDrawCommandQueue()->ExecuteCommandLists(UINT(std::size(lists)), lists);
    d3d12->flush();
    commandAllocator->Reset();
    commandList->Reset(commandAllocator.Get(), nullptr);
}

ComPtr<ID3D12Resource> ModuleGPUResources::getUploadHeap(size_t size){
    ComPtr<ID3D12Resource> uploadHeap;
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    app->getD3D12()->getDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadHeap));
    return uploadHeap;
}

ComPtr<ID3D12Resource> ModuleGPUResources::createUploadBuffer(const void* data, size_t size, const char* name){
    ComPtr<ID3D12Resource> buffer;
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    app->getD3D12()->getDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer));
    buffer->SetName(toWString(name).c_str());

    BYTE* pData = nullptr;
    CD3DX12_RANGE readRange(0, 0);
    buffer->Map(0, &readRange, reinterpret_cast<void**>(&pData));
    memcpy(pData, data, size);
    buffer->Unmap(0, nullptr);
    return buffer;
}

ComPtr<ID3D12Resource> ModuleGPUResources::createDefaultBuffer(const void* data, size_t size, const char* name){
    ComPtr<ID3D12Resource> buffer;
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    bool ok = SUCCEEDED(app->getD3D12()->getDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffer)));

    ComPtr<ID3D12Resource> upload = getUploadHeap(size);

    if (ok)
    {
        BYTE* pData = nullptr;
        CD3DX12_RANGE readRange(0, 0);
        upload->Map(0, &readRange, reinterpret_cast<void**>(&pData));
        memcpy(pData, data, size);
        upload->Unmap(0, nullptr);
        commandList->CopyBufferRegion(buffer.Get(), 0, upload.Get(), 0, size);
        executeAndFlush();
        buffer->SetName(toWString(name).c_str());
    }
    return buffer;
}

ComPtr<ID3D12Resource> ModuleGPUResources::createRawTexture2D(const void* data, size_t rowSize, size_t width, size_t height, DXGI_FORMAT format){
    ID3D12Device2* device = app->getD3D12()->getDevice();

    D3D12_RESOURCE_DESC desc = {};
    desc.Width = UINT(width); desc.Height = UINT(height);
    desc.MipLevels = 1; desc.DepthOrArraySize = 1; desc.Format = format;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE; desc.SampleDesc.Count = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    ComPtr<ID3D12Resource> texture;
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    bool ok = SUCCEEDED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture)));

    UINT64 requiredSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
    device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, nullptr, nullptr, &requiredSize);
    ComPtr<ID3D12Resource> upload = getUploadHeap(requiredSize);

    if (ok){
        BYTE* uploadData = nullptr;
        CD3DX12_RANGE readRange(0, 0);
        upload->Map(0, &readRange, reinterpret_cast<void**>(&uploadData));
        for (uint32_t i = 0; i < height; ++i)
            memcpy(uploadData + layout.Offset + layout.Footprint.RowPitch * i, (BYTE*)data + rowSize * i, rowSize);
        upload->Unmap(0, nullptr);

        CD3DX12_TEXTURE_COPY_LOCATION dst(texture.Get(), 0);
        CD3DX12_TEXTURE_COPY_LOCATION src(upload.Get(), layout);
        commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &barrier);
        executeAndFlush();
    }
    return texture;
}

ComPtr<ID3D12Resource> ModuleGPUResources::createTextureFromMemory(const void* data, size_t size, const char* name){
    ScratchImage image;
    bool ok = SUCCEEDED(LoadFromDDSMemory(data, size, DDS_FLAGS_NONE, nullptr, image));
    ok = ok || SUCCEEDED(LoadFromHDRMemory(data, size, nullptr, image));
    ok = ok || SUCCEEDED(LoadFromTGAMemory(data, size, TGA_FLAGS_NONE, nullptr, image));
    ok = ok || SUCCEEDED(LoadFromWICMemory(data, size, WIC_FLAGS_NONE, nullptr, image));
    return ok ? createTextureFromImage(image, name) : nullptr;
}

ComPtr<ID3D12Resource> ModuleGPUResources::createTextureFromFile(const std::filesystem::path& path, bool defaultSRGB){
    const wchar_t* fileName = path.c_str();
    ScratchImage image;
    bool ok = SUCCEEDED(LoadFromDDSFile(fileName, DDS_FLAGS_NONE, nullptr, image));
    ok = ok || SUCCEEDED(LoadFromHDRFile(fileName, nullptr, image));
    ok = ok || SUCCEEDED(LoadFromTGAFile(fileName, defaultSRGB ? TGA_FLAGS_DEFAULT_SRGB : TGA_FLAGS_NONE, nullptr, image));
    ok = ok || SUCCEEDED(LoadFromWICFile(fileName, defaultSRGB ? WIC_FLAGS_DEFAULT_SRGB : WIC_FLAGS_NONE, nullptr, image));
    return ok ? createTextureFromImage(image, path.string().c_str()) : nullptr;
}

ComPtr<ID3D12Resource> ModuleGPUResources::createTextureFromImage(const ScratchImage& image, const char* name){
    ID3D12Device2* device = app->getD3D12()->getDevice();
    const TexMetadata& meta = image.GetMetadata();
    _ASSERTE(meta.dimension == TEX_DIMENSION_TEXTURE2D);
    if (meta.dimension != TEX_DIMENSION_TEXTURE2D) return {};

    ComPtr<ID3D12Resource> texture;
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(meta.format, UINT64(meta.width), UINT(meta.height), UINT16(meta.arraySize), UINT16(meta.mipLevels));
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    bool ok = SUCCEEDED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture)));

    ComPtr<ID3D12Resource> upload;
    if (ok){
        _ASSERTE(meta.mipLevels * meta.arraySize == image.GetImageCount());
        upload = getUploadHeap(GetRequiredIntermediateSize(texture.Get(), 0, UINT(image.GetImageCount())));
        ok = upload != nullptr;
    }

    if (ok){
        std::vector<D3D12_SUBRESOURCE_DATA> subData;
        subData.reserve(image.GetImageCount());
        for (size_t item = 0; item < meta.arraySize; ++item)
            for (size_t level = 0; level < meta.mipLevels; ++level)
            {
                const Image* subImg = image.GetImage(level, item, 0);
                subData.push_back({ subImg->pixels, (LONG_PTR)subImg->rowPitch, (LONG_PTR)subImg->slicePitch });
            }
        ok = UpdateSubresources(commandList.Get(), texture.Get(), upload.Get(), 0, 0, UINT(image.GetImageCount()), subData.data()) != 0;
    }

    if (ok){
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &barrier);
        executeAndFlush();
        texture->SetName(toWString(name).c_str());
        return texture;
    }
    return {};
}

ComPtr<ID3D12Resource> ModuleGPUResources::createRenderTarget(DXGI_FORMAT format, size_t width, size_t height, size_t arraySize, size_t mipLevels, UINT sampleCount, const Vector4& clearColour, const char* name){
    ComPtr<ID3D12Resource> texture;
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format, UINT64(width), UINT(height), UINT16(arraySize), UINT16(mipLevels), sampleCount, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    D3D12_CLEAR_VALUE clearValue = { format, {clearColour.x, clearColour.y, clearColour.z, clearColour.w} };
    app->getD3D12()->getDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, &desc, D3D12_RESOURCE_STATE_COMMON, &clearValue, IID_PPV_ARGS(&texture));
    texture->SetName(toWString(name).c_str());
    return texture;
}

ComPtr<ID3D12Resource> ModuleGPUResources::createDepthStencil(DXGI_FORMAT format, size_t width, size_t height, UINT sampleCount, float clearDepth, uint8_t clearStencil, const char* name){
    ComPtr<ID3D12Resource> texture;
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format, UINT64(width), UINT(height), 1, 1, sampleCount, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    D3D12_CLEAR_VALUE clear = { format, {} };
    clear.DepthStencil.Depth = clearDepth;
    clear.DepthStencil.Stencil = clearStencil;
    app->getD3D12()->getDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&texture));
    texture->SetName(toWString(name).c_str());
    return texture;
}

void ModuleGPUResources::preRender(){
    UINT completedFrame = app->getD3D12()->getLastCompletedFrame();
    for (int i = 0; i < (int)deferredFrees.size();){
        if (completedFrame >= deferredFrees[i].frame) { deferredFrees[i] = deferredFrees.back(); deferredFrees.pop_back(); }
        else ++i;
    }
}

void ModuleGPUResources::deferRelease(ComPtr<ID3D12Resource> resource){
    if (!resource) return;
    UINT currentFrame = app->getD3D12()->getCurrentFrame();
    auto it = std::find_if(deferredFrees.begin(), deferredFrees.end(), [&resource](const DeferredFree& item) { return item.resource.Get() == resource.Get(); });
    if (it != deferredFrees.end()) it->frame = currentFrame;
    else deferredFrees.push_back({ currentFrame, resource });
}