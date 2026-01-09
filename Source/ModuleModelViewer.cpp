#include "Globals.h"
#include "ModuleModelViewer.h"

#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleCamera.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleResources.h"
#include "GraphicsSamplers.h"
#include <imgui.h>
#include "ImGuizmo.h"

#include "Model.h"
#include "Mesh.h"
#include "Material.h"

#include "ReadData.h"

#include "DirectXTex.h"
#include <d3d12.h>
#include "d3dx12.h"


ModuleModelViewer::ModuleModelViewer()
{
}

ModuleModelViewer::~ModuleModelViewer()
{
}

bool ModuleModelViewer::init()
{
    bool ok = createRootSignature();
    ok = ok && createPSO();
    ok = ok && loadModel();

    if (ok)
    {
        ModuleD3D12* d3d12 = app->getD3D12();
        debugDrawPass = std::make_unique<DebugDrawPass>(d3d12->getDevice(), d3d12->getDrawCommandQueue(), false);
    }

    return ok;
}

bool ModuleModelViewer::cleanUp()
{
    debugDrawPass.reset();

    materialBuffers.clear();
    model.reset();

    rootSignature.Reset();
    pso.Reset();

    return true;
}

void ModuleModelViewer::preRender()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    if (d3d12 && showGuizmo && model)
    {
        unsigned width = d3d12->getWindowWidth();
        unsigned height = d3d12->getWindowHeight();
        ImGuizmo::SetRect(0, 0, float(width), float(height));
    }
}

void ModuleModelViewer::render()
{
}

void ModuleModelViewer::render3DContent(ID3D12GraphicsCommandList* commandList)
{
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleCamera* camera = app->getCamera();
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    GraphicsSamplers* samplers = app->getGraphicsSamplers();

    if (!d3d12 || !camera || !model || !commandList) return;

    unsigned width = d3d12->getWindowWidth();
    unsigned height = d3d12->getWindowHeight();

    const Matrix& view = camera->getView();
    Matrix proj = ModuleCamera::getPerspectiveProj(float(width) / float(height));

    if (showGuizmo)
    {
        Matrix objectMatrix = model->getModelMatrix();

        float matrix[16];
        memcpy(matrix, &objectMatrix, sizeof(float) * 16);

        ImGuizmo::Manipulate(
            (const float*)&view,
            (const float*)&proj,
            gizmoOperation,
            ImGuizmo::LOCAL,
            matrix
        );

        if (ImGuizmo::IsUsing())
        {
            memcpy(&objectMatrix, matrix, sizeof(float) * 16);
            model->setModelMatrix(objectMatrix);
        }
    }

    Matrix mvp = model->getModelMatrix() * view * proj;
    mvp = mvp.Transpose();

    D3D12_VIEWPORT viewport;
    viewport.TopLeftX = viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.Width = float(width);
    viewport.Height = float(height);

    D3D12_RECT scissor;
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = width;
    scissor.bottom = height;

    commandList->SetPipelineState(pso.Get());
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (descriptors && samplers)
    {
        ID3D12DescriptorHeap* descriptorHeaps[] = {
            descriptors->getDescriptorHeap(),
            samplers->getHeap()
        };
        commandList->SetDescriptorHeaps(2, descriptorHeaps);
        commandList->SetGraphicsRootDescriptorTable(3,
            samplers->getGPUHandle(GraphicsSamplers::LINEAR_WRAP));
    }

    commandList->SetGraphicsRoot32BitConstants(0,
        sizeof(Matrix) / sizeof(UINT32), &mvp, 0);

    BEGIN_EVENT(commandList, "Model Render Pass");

    const auto& meshes = model->getMeshes();
    const auto& materials = model->getMaterials();

    for (size_t i = 0; i < meshes.size(); ++i)
    {
        const auto& mesh = meshes[i];
        int materialIndex = mesh->getMaterialIndex();

        if (materialIndex >= 0 && materialIndex < (int)materials.size() &&
            materialIndex < (int)materialBuffers.size())
        {
            const auto& material = materials[materialIndex];

            if (materialBuffers[materialIndex])
            {
                commandList->SetGraphicsRootConstantBufferView(1,
                    materialBuffers[materialIndex]->GetGPUVirtualAddress());
            }

            D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = material->getTextureGPUHandle();
            if (textureHandle.ptr != 0)
            {
                commandList->SetGraphicsRootDescriptorTable(2, textureHandle);
            }

            mesh->draw(commandList);
        }
        else
        {
            mesh->draw(commandList);
        }
    }

    END_EVENT(commandList);

    if (showGrid && debugDrawPass)
        dd::xzSquareGrid(-10.0f, 10.0f, 0.0f, 1.0f, dd::colors::LightGray);
    if (showAxis && debugDrawPass)
        dd::axisTriad(ddConvert(Matrix::Identity), 0.1f, 1.0f);

    if (debugDrawPass)
        debugDrawPass->record(commandList, width, height, view, proj);
}

bool ModuleModelViewer::createRootSignature()
{
    auto device = app->getD3D12()->getDevice();
    if (!device) return false;

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    CD3DX12_ROOT_PARAMETER rootParameters[4] = {};
    CD3DX12_DESCRIPTOR_RANGE tableRanges;
    CD3DX12_DESCRIPTOR_RANGE sampRange;

    tableRanges.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, GraphicsSamplers::COUNT, 0);

    rootParameters[0].InitAsConstants((sizeof(Matrix) / sizeof(UINT32)), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[2].InitAsDescriptorTable(1, &tableRanges, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[3].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);

    rootSignatureDesc.Init(4, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> rootSignatureBlob;
    ComPtr<ID3DBlob> errorBlob;

    if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        &rootSignatureBlob, &errorBlob)))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        return false;
    }

    if (FAILED(device->CreateRootSignature(0,
        rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature))))
    {
        return false;
    }

    return true;
}

bool ModuleModelViewer::loadModel()
{
    model = std::make_unique<Model>();

    // Try different models
    if (!model->load("Assets/Models/Duck/duck.gltf", "Assets/Models/Duck/"))
    {
        if (!model->load("Assets/Models/BoxTextured/BoxTextured.gltf", "Assets/Models/BoxTextured/"))
        {
            if (!model->load("Assets/Models/BoxInterleaved/BoxInterleaved.gltf", "Assets/Models/BoxInterleaved/"))
            {
                LOG("Failed to load any model!");
                return false;
            }
        }
    }

    model->setModelMatrix(Matrix::CreateScale(0.01f, 0.01f, 0.01f));

    ModuleResources* resources = app->getResources();
    if (!resources) return false;

    const auto& materials = model->getMaterials();
    materialBuffers.resize(materials.size());

    for (size_t i = 0; i < materials.size(); ++i)
    {
        const auto& material = materials[i];
        const Material::Data& data = material->getData();

        materialBuffers[i] = resources->createDefaultBuffer(
            &data,
            sizeof(Material::Data),
            material->getName()
        );
    }

    LOG("Loaded model with %zu meshes and %zu materials",
        model->getMeshes().size(),
        materials.size());

    return true;
}

bool ModuleModelViewer::createPSO()
{
    auto device = app->getD3D12()->getDevice();
    if (!device) return false;

    const D3D12_INPUT_ELEMENT_DESC* inputLayout = Mesh::getInputLayout();
    uint32_t inputLayoutCount = Mesh::getInputLayoutCount();

    auto dataVS = DX::ReadData(L"ModelSamplerVS.cso");
    auto dataPS = DX::ReadData(L"ModelSamplerPS.cso");

    if (dataVS.empty() || dataPS.empty())
    {
        dataVS = DX::ReadData(L"Exercise5VS.cso");
        dataPS = DX::ReadData(L"Exercise5PS.cso");

        if (dataVS.empty() || dataPS.empty())
        {
            LOG("Shader files not found! Please compile ModelViewerVS.hlsl and ModelViewerPS.hlsl");
            return false;
        }
    }

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


    HRESULT hr = device->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&pso)
    );

    if (FAILED(hr))
    {
        LOG("Failed to create PSO: 0x%08X", hr);
        return false;
    }

    return true;
}