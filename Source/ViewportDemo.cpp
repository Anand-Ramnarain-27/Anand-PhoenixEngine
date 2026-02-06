#include "Globals.h"
#include "ViewportDemo.h"

#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleCamera.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "GraphicsSamplers.h"
#include "ModuleRingBuffer.h"
#include "ModuleResources.h"
#include "Model.h"
#include "Mesh.h"
#include "RenderTexture.h"

#include "ReadData.h"

#include "DirectXTex.h"
#include <d3d12.h>
#include "d3dx12.h"

static Matrix getNormalMatrix(const Matrix& modelMatrix)
{
    return modelMatrix.Invert().Transpose();
}

ViewportDemo::ViewportDemo()
{
}

ViewportDemo::~ViewportDemo()
{
}

bool ViewportDemo::init()
{
    bool ok = createRootSignature();
    ok = ok && createPSO();
    ok = ok && loadModel();

    if (ok)
    {
        ModuleD3D12* d3d12 = app->getD3D12();
        ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();

        descTable = descriptors->allocTable();
        imguiPass = std::make_unique<ImGuiPass>(d3d12->getDevice(), d3d12->getHWnd(),
            descTable.getCPUHandle(), descTable.getGPUHandle());

        // Create render texture for rendering the scene
        renderTexture = std::make_unique<RenderTexture>("ViewportDemo",
            DXGI_FORMAT_R8G8B8A8_UNORM,
            Vector4(0.188f, 0.208f, 0.259f, 1.0f),
            DXGI_FORMAT_D32_FLOAT,
            1.0f);

        // Initialize with a default size
        renderTexture->resize(800, 600);
    }

    return true;
}

bool ViewportDemo::cleanUp()
{
    // Wait for GPU before cleaning up
    ModuleD3D12* d3d12 = app->getD3D12();
    if (d3d12)
    {
        d3d12->flush();
    }

    imguiPass.reset();
    debugDrawPass.reset();
    model.reset();
    renderTexture.reset();

    return true;
}

void ViewportDemo::preRender()
{
    imguiPass->startFrame();

    // Wait for GPU to finish before resizing
    ModuleD3D12* d3d12 = app->getD3D12();

    // Only resize if we have a valid canvas size
    if (canvasSize.x > 0.0f && canvasSize.y > 0.0f)
    {
        // Check if we need to resize
        int currentWidth = renderTexture->getWidth();
        int currentHeight = renderTexture->getHeight();
        int targetWidth = int(canvasSize.x);
        int targetHeight = int(canvasSize.y);

        if (targetWidth != currentWidth || targetHeight != currentHeight)
        {
            // Wait for GPU to finish using current texture
            d3d12->flush();  // This is the key fix!

            // Now it's safe to resize
            renderTexture->resize(targetWidth, targetHeight);
        }
    }
}

void ViewportDemo::imGuiCommands()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleCamera* camera = app->getCamera();

    ImGui::Begin("Scene View");

    // Calculate available content area for the viewport
    ImVec2 max = ImGui::GetWindowContentRegionMax();
    ImVec2 min = ImGui::GetWindowContentRegionMin();
    canvasPos = min;
    canvasSize = ImVec2(max.x - min.x, max.y - min.y);  // This is the resizing part

    // Display the rendered texture in ImGui with the calculated size
    if (renderTexture->getSrvTableDesc().isValid())
    {
        ImGui::Image((ImTextureID)renderTexture->getSrvHandle().ptr, canvasSize);
    }

    ImGui::End();

    // Let the camera respond to input only when the scene view is focused
    ImGuiIO& io = ImGui::GetIO();
    bool viewerFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    camera->setEnable(viewerFocused);
}

void ViewportDemo::render()
{
    imGuiCommands();

    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    GraphicsSamplers* samplers = app->getGraphicsSamplers();

    ID3D12GraphicsCommandList* commandList = d3d12->getCommandList();

    commandList->Reset(d3d12->getCommandAllocator(), nullptr);

    commandList->SetGraphicsRootSignature(rootSignature.Get());
    ID3D12DescriptorHeap* descriptorHeaps[] = { descriptors->getHeap(), samplers->getHeap() };
    commandList->SetDescriptorHeaps(2, descriptorHeaps);

    // Render to texture if we have a valid canvas size
    if (renderTexture->isValid() && canvasSize.x > 0.0f && canvasSize.y > 0.0f)
    {
        renderToTexture(commandList);
    }

    // Transition back buffer to render target for ImGui
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d12->getRenderTargetDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = d3d12->getDepthStencilDescriptor();

    commandList->OMSetRenderTargets(1, &rtv, false, nullptr);

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    // Render ImGui on top
    imguiPass->record(commandList);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    commandList->ResourceBarrier(1, &barrier);

    if (SUCCEEDED(commandList->Close()))
    {
        ID3D12CommandList* commandLists[] = { commandList };
        d3d12->getDrawCommandQueue()->ExecuteCommandLists(UINT(std::size(commandLists)), commandLists);
    }
}

void ViewportDemo::renderToTexture(ID3D12GraphicsCommandList* commandList)
{
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleCamera* camera = app->getCamera();
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    GraphicsSamplers* samplers = app->getGraphicsSamplers();
    ModuleRingBuffer* ringBuffer = app->getRingBuffer();

    commandList->SetPipelineState(pso.Get());

    unsigned width = unsigned(canvasSize.x);
    unsigned height = unsigned(canvasSize.y);

    const Matrix& view = camera->getView();
    Matrix proj = ModuleCamera::getPerspectiveProj(float(width) / float(height));

    Matrix mvp = model->getModelMatrix() * view * proj;
    mvp = mvp.Transpose();

    PerFrame perFrame;
    perFrame.L = light.L;
    perFrame.Lc = light.Lc;
    perFrame.Ac = light.Ac;
    perFrame.viewPos = camera->getPos();

    perFrame.L.Normalize();

    // Begin rendering to the render texture
    renderTexture->beginRender(commandList);

    commandList->SetGraphicsRoot32BitConstants(0, sizeof(Matrix) / sizeof(UINT32), &mvp, 0);
    commandList->SetGraphicsRootConstantBufferView(1, ringBuffer->allocateConstantBuffer(&perFrame, sizeof(PerFrame)));
    commandList->SetGraphicsRootDescriptorTable(4, samplers->getGPUHandle(GraphicsSamplers::LINEAR_WRAP));

    BEGIN_EVENT(commandList, "Model Render Pass");

    // SAFER VERSION: Check all conditions carefully
    const auto& meshes = model->getMeshes();
    const auto& materials = model->getMaterials();

    for (const auto& mesh : meshes)
    {
        int materialIndex = mesh->getMaterialIndex();

        // More robust check
        bool hasValidMaterial = false;
        const Material* materialPtr = nullptr;

        if (materialIndex >= 0 && materialIndex < static_cast<int>(materials.size()))
        {
            if (!materials.empty())
            {
                materialPtr = materials[materialIndex].get();
                hasValidMaterial = true;
            }
        }

        PerInstance perInstance;
        perInstance.modelMat = model->getModelMatrix().Transpose();
        perInstance.normalMat = getNormalMatrix(model->getModelMatrix()).Transpose();

        if (hasValidMaterial && materialPtr)
        {
            perInstance.material = materialPtr->getData();

            commandList->SetGraphicsRootConstantBufferView(2,
                ringBuffer->allocateConstantBuffer(&perInstance, sizeof(PerInstance)));

            D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = materialPtr->getTextureGPUHandle();
            commandList->SetGraphicsRootDescriptorTable(3, textureHandle);
        }
        else
        {
            // Use default material if no valid material found
            Material::Data defaultMaterial;
            defaultMaterial.baseColour = Vector4(0.8f, 0.8f, 0.8f, 1.0f);
            defaultMaterial.hasColourTexture = FALSE;
            perInstance.material = defaultMaterial;

            commandList->SetGraphicsRootConstantBufferView(2,
                ringBuffer->allocateConstantBuffer(&perInstance, sizeof(PerInstance)));

            // Use null descriptor for texture
            D3D12_GPU_DESCRIPTOR_HANDLE nullHandle = {};
            commandList->SetGraphicsRootDescriptorTable(3, nullHandle);
        }

        mesh->draw(commandList);
    }

    END_EVENT(commandList);

    // End rendering to the render texture
    renderTexture->endRender(commandList);
}

bool ViewportDemo::createRootSignature()
{
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    CD3DX12_ROOT_PARAMETER rootParameters[5] = {};
    CD3DX12_DESCRIPTOR_RANGE tableRanges;
    CD3DX12_DESCRIPTOR_RANGE sampRange;

    tableRanges.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, GraphicsSamplers::COUNT, 0);

    rootParameters[0].InitAsConstants((sizeof(Matrix) / sizeof(UINT32)), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[2].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[3].InitAsDescriptorTable(1, &tableRanges, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[4].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);

    rootSignatureDesc.Init(5, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> rootSignatureBlob;

    if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, nullptr)))
    {
        return false;
    }

    if (FAILED(app->getD3D12()->getDevice()->CreateRootSignature(0,
        rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature))))
    {
        return false;
    }

    return true;
}

bool ViewportDemo::createPSO()
{
    const D3D12_INPUT_ELEMENT_DESC* inputLayout = Mesh::getInputLayout();
    UINT inputLayoutCount = Mesh::getInputLayoutCount();

    auto dataVS = DX::ReadData(L"RenderToTextureDemoVS.cso");
    auto dataPS = DX::ReadData(L"RenderToTextureDemoPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputLayout, inputLayoutCount };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = { dataVS.data(), dataVS.size() };
    psoDesc.PS = { dataPS.data(), dataPS.size() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc = { 1, 0 };
    psoDesc.SampleMask = 0xffffffff;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.NumRenderTargets = 1;

    return SUCCEEDED(app->getD3D12()->getDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
}

bool ViewportDemo::loadModel()
{
    model = std::make_unique<Model>();
    bool loaded = model->load("Assets/Models/Duck/duck.gltf", "Assets/Models/Duck/");
    if (!loaded)
    {
        LOG("Failed to load model");
        return false;
    }

    model->setModelMatrix(Matrix::CreateScale(0.01f, 0.01f, 0.01f));

    return true;
}