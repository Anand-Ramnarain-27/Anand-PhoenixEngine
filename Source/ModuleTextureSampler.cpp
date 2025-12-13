#include "Globals.h"
#include "ModuleTextureSampler.h"

#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleCamera.h"
#include "ModuleResources.h"
#include "ModuleEditor.h" 

#include "ReadData.h"

#include "DirectXTex.h"
#include <d3d12.h>
#include "d3dx12.h"

#include <imgui.h>

bool ModuleTextureSampler::init()
{
    struct Vertex
    {
        Vector3 position;
        Vector2 uv;
    };

    static Vertex vertices[6] =
    {
        { Vector3(-1.0f, -1.0f, 0.0f),  Vector2(-0.2f, 1.2f) },
        { Vector3(-1.0f, 1.0f, 0.0f),   Vector2(-0.2f, -0.2f) },
        { Vector3(1.0f, 1.0f, 0.0f),    Vector2(1.2f, -0.2f) },
        { Vector3(-1.0f, -1.0f, 0.0f),  Vector2(-0.2f, 1.2f) },
        { Vector3(1.0f, 1.0f, 0.0f),    Vector2(1.2f, -0.2f) },
        { Vector3(1.0f, -1.0f, 0.0f),   Vector2(1.2f, 1.2f) }
    };

    bool ok = createVertexBuffer(&vertices[0], sizeof(vertices), sizeof(Vertex));

    ok = ok && createRootSignature();
    ok = ok && createPSO();

    ok = ok && loadTexture();

    if (ok)
    {
        ModuleD3D12* d3d12 = app->getD3D12();
        debugDrawPass = std::make_unique<DebugDrawPass>(d3d12->getDevice(), d3d12->getDrawCommandQueue(), false);
    }

    return ok;
}

bool ModuleTextureSampler::cleanUp()
{
    imguiPass.reset();
    debugDrawPass.reset();
    return true;
}

void ModuleTextureSampler::preRender()
{

}

void ModuleTextureSampler::render()
{

}

void ModuleTextureSampler::render3DContent(ID3D12GraphicsCommandList* commandList)
{
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleCamera* camera = app->getCamera();
    GraphicsSamplers* samplers = app->getGraphicsSamplers();
    ModuleEditor* editor = app->getEditor();

    //ID3D12GraphicsCommandList* commandList = d3d12->beginFrameRender();
    //d3d12->setBackBufferRenderTarget(Vector4(0.2f, 0.2f, 0.2f, 1.0f));

    if (!commandList) return;

    unsigned width = d3d12->getWindowWidth();
    unsigned height = d3d12->getWindowHeight();

    Matrix model = Matrix::Identity;
    const Matrix& view = camera->getView();
    Matrix proj = ModuleCamera::getPerspectiveProj(float(width) / float(height));
    Matrix mvp = model * view * proj;
    mvp = mvp.Transpose();

    commandList->SetPipelineState(pso.Get());
    commandList->SetGraphicsRootSignature(rootSignature.Get());

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

    ID3D12DescriptorHeap* descriptorHeaps[] = { srvHeap.Get(), samplers->getHeap() };
    commandList->SetDescriptorHeaps(2, descriptorHeaps);

    commandList->SetGraphicsRoot32BitConstants(0, sizeof(Matrix) / sizeof(UINT32), &mvp, 0);
    commandList->SetGraphicsRootDescriptorTable(1, srvGPUHandle);

    GraphicsSamplers::Type samplerType = GraphicsSamplers::LINEAR_WRAP;
    if (editor)
    {
        samplerType = editor->GetTextureFilter();
    }

    D3D12_GPU_DESCRIPTOR_HANDLE samplerHandle = samplers->getGPUHandle(samplerType);
    commandList->SetGraphicsRootDescriptorTable(2, samplerHandle);

    commandList->DrawInstanced(6, 1, 0, 0);

    bool showGrid = true;
    bool showAxis = true;
    if (editor)
    {
        showGrid = editor->IsGridVisible();
        showAxis = editor->IsAxisVisible();
    }

    if (showGrid) dd::xzSquareGrid(-10.0f, 10.0f, 0.0f, 1.0f, dd::colors::LightGray);
    if (showAxis) dd::axisTriad(ddConvert(Matrix::Identity), 0.1f, 1.0f);

    if (debugDrawPass)
        debugDrawPass->record(commandList, width, height, view, proj);

    if (imguiPass)
        imguiPass->record(commandList);

    //d3d12->endFrameRender();

}

bool ModuleTextureSampler::createVertexBuffer(void* bufferData, unsigned bufferSize, unsigned stride)
{
    auto device = app->getD3D12()->getDevice();

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    if (FAILED(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer))))
    {
        return false;
    }

    void* vertexData;
    vertexBuffer->Map(0, nullptr, &vertexData);
    memcpy(vertexData, bufferData, bufferSize);
    vertexBuffer->Unmap(0, nullptr);

    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = stride;
    vertexBufferView.SizeInBytes = bufferSize;

    return true;
}

bool ModuleTextureSampler::createRootSignature()
{
    auto device = app->getD3D12()->getDevice();

    CD3DX12_ROOT_PARAMETER rootParameters[3] = {};
    CD3DX12_DESCRIPTOR_RANGE srvRange, sampRange;

    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);   
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0); 

    rootParameters[0].InitAsConstants(sizeof(Matrix) / sizeof(UINT32), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[2].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(3, rootParameters, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;

    if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        &signature, &error)))
    {
        if (error)
            OutputDebugStringA((char*)error->GetBufferPointer());
        return false;
    }

    return SUCCEEDED(device->CreateRootSignature(0, signature->GetBufferPointer(),
        signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
}

bool ModuleTextureSampler::createPSO()
{
    auto device = app->getD3D12()->getDevice();

    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    auto dataVS = DX::ReadData(L"TextureSamplerVS.cso");
    auto dataPS = DX::ReadData(L"TextureSamplerPS.cso");

    if (dataVS.empty() || dataPS.empty())
        return false;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputLayout, sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = { dataVS.data(), dataVS.size() };
    psoDesc.PS = { dataPS.data(), dataPS.size() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc = { 1, 0 };
    psoDesc.SampleMask = 0xffffffff;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.NumRenderTargets = 1;

    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
}

bool ModuleTextureSampler::loadTexture()
{
    auto device = app->getD3D12()->getDevice();
    auto d3d12 = app->getD3D12();

    wchar_t currentDir[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, currentDir);
    LOG("Current directory: %S", currentDir);

    wchar_t fullPath[MAX_PATH];
    GetFullPathNameW(L"Assets/Textures/dog.dds", MAX_PATH, fullPath, nullptr);
    LOG("Looking for texture at: %S", fullPath);

    DirectX::ScratchImage image;
    HRESULT hr = DirectX::LoadFromDDSFile(L"Assets/Textures/dog.dds", DirectX::DDS_FLAGS_NONE, nullptr, image);

    if (FAILED(hr))
    {
        LOG("Failed to load texture: dog.dds (Error: 0x%08X)", hr);

        DirectX::ScratchImage defaultImage;
        const UINT64 width = 256;
        const UINT height = 256;

        if (SUCCEEDED(defaultImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1)))
        {
            auto pixels = reinterpret_cast<uint32_t*>(defaultImage.GetPixels());
            for (UINT y = 0; y < height; ++y)
            {
                for (UINT x = 0; x < width; ++x)
                {
                    bool checker = ((x / 32) % 2) ^ ((y / 32) % 2);
                    pixels[y * width + x] = checker ? 0xFF00FF00 : 0xFF0000FF;
                }
            }
            image = std::move(defaultImage);
        }
        else
        {
            return false;
        }
    }

    const auto& metadata = image.GetMetadata();

    LOG("Texture loaded successfully:");
    LOG("  Format: %d (DXGI_FORMAT)", metadata.format);
    LOG("  Width: %u", metadata.width);
    LOG("  Height: %u", metadata.height);
    LOG("  MipLevels: %u", metadata.mipLevels);
    LOG("  ArraySize: %u", metadata.arraySize);
    LOG("  Image count: %zu", image.GetImageCount());

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        metadata.format,
        static_cast<UINT64>(metadata.width),
        static_cast<UINT>(metadata.height),
        static_cast<UINT16>(metadata.arraySize),
        static_cast<UINT16>(metadata.mipLevels));

    if (FAILED(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&textureDog))))
    {
        LOG("Failed to create texture resource");
        return false;
    }

    LOG("Copying texture data to GPU (with mipmaps)...");

    // Get total number of subresources (all mip levels)
    size_t imageCount = image.GetImageCount();
    LOG("  Total images (subresources): %zu", imageCount);

    // Create upload heap for copying
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(textureDog.Get(), 0, (UINT)imageCount);
    LOG("  Required upload buffer size: %llu bytes", uploadBufferSize);

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    ComPtr<ID3D12Resource> stagingBuffer;
    if (FAILED(device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&stagingBuffer))))
    {
        LOG("Failed to create staging buffer");
        return false;
    }

    // Prepare subresource data for ALL mip levels
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    subresources.reserve(imageCount);

    for (size_t i = 0; i < imageCount; ++i)
    {
        const DirectX::Image* img = image.GetImage(i, 0, 0);
        if (!img || !img->pixels)
        {
            LOG("ERROR: Image %zu is null!", i);
            return false;
        }

        D3D12_SUBRESOURCE_DATA data = {};
        data.pData = img->pixels;
        data.RowPitch = img->rowPitch;
        data.SlicePitch = img->slicePitch;

        subresources.push_back(data);

        LOG("  Subresource %zu: RowPitch=%u, SlicePitch=%u",
            i, img->rowPitch, img->slicePitch);
    }

    ComPtr<ID3D12CommandAllocator> uploadAllocator;
    ComPtr<ID3D12GraphicsCommandList> uploadCommandList;
    ComPtr<ID3D12Fence> uploadFence;
    HANDLE uploadEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    UINT64 uploadFenceValue = 1;

    // Create upload command allocator and list
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&uploadAllocator))))
    {
        LOG("Failed to create upload command allocator");
        return false;
    }

    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        uploadAllocator.Get(), nullptr, IID_PPV_ARGS(&uploadCommandList))))
    {
        LOG("Failed to create upload command list");
        return false;
    }

    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&uploadFence))))
    {
        LOG("Failed to create upload fence");
        return false;
    }

    UpdateSubresources(uploadCommandList.Get(), textureDog.Get(), stagingBuffer.Get(),
        0, 0, (UINT)imageCount, subresources.data());

    // Transition to pixel shader resource
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        textureDog.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    uploadCommandList->ResourceBarrier(1, &barrier);

    uploadCommandList->Close();

    ID3D12CommandList* commandLists[] = { uploadCommandList.Get() };
    d3d12->getDrawCommandQueue()->ExecuteCommandLists(1, commandLists);

    d3d12->getDrawCommandQueue()->Signal(uploadFence.Get(), uploadFenceValue);

    if (uploadFence->GetCompletedValue() < uploadFenceValue)
    {
        uploadFence->SetEventOnCompletion(uploadFenceValue, uploadEvent);
        WaitForSingleObject(uploadEvent, INFINITE);
    }

    CloseHandle(uploadEvent);

    LOG("Texture data copied to GPU successfully (%zu mip levels)", imageCount);

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap))))
    {
        LOG("Failed to create SRV descriptor heap");
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = metadata.format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);
    srvDesc.Texture2D.MostDetailedMip = 0;

    device->CreateShaderResourceView(textureDog.Get(), &srvDesc,
        srvHeap->GetCPUDescriptorHandleForHeapStart());

    srvGPUHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();

    return true;
}