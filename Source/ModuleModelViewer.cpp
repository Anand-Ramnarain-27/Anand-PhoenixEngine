#include "Globals.h"
#include "ModuleModelViewer.h"

#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleCamera.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleResources.h"
#include "GraphicsSamplers.h"
#include "ModuleRingBuffer.h"
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

Matrix ModuleModelViewer::getNormalMatrix() const
{
    Matrix modelMatrix = model->getModelMatrix();
    modelMatrix.Invert();
    modelMatrix = modelMatrix.Transpose();
    return modelMatrix;
}

void ModuleModelViewer::render3DContent(ID3D12GraphicsCommandList* cmd)
{
    auto d3d12 = app->getD3D12();
    auto camera = app->getCamera();
    auto descriptors = app->getShaderDescriptors();
    auto samplers = app->getGraphicsSamplers();
    auto ringBuffer = app->getRingBuffer();

    if (!cmd || !model || !camera) return;

    const unsigned w = d3d12->getWindowWidth();
    const unsigned h = d3d12->getWindowHeight();

    const Matrix& view = camera->getView();
    Matrix proj = ModuleCamera::getPerspectiveProj(float(w) / float(h));

    if (showGuizmo)
    {
        Matrix m = model->getModelMatrix();
        ImGuizmo::Manipulate(
            (float*)&view,
            (float*)&proj,
            gizmoOperation,
            ImGuizmo::LOCAL,
            (float*)&m
        );

        if (ImGuizmo::IsUsing())
            model->setModelMatrix(m);
    }

    PerFrame pf{};
    pf.L = light.L;
    pf.Lc = light.Lc;
    pf.Ac = light.Ac;
    pf.viewPos = camera->getPos();

    RingBufferAllocation frameAlloc = ringBuffer->allocateType(pf);

    Matrix mvp = model->getModelMatrix() * view * proj;
    mvp = mvp.Transpose();

    D3D12_VIEWPORT vp{ 0,0,float(w),float(h),0,1 };
    D3D12_RECT sc{ 0,0,(LONG)w,(LONG)h };

    cmd->SetPipelineState(pso.Get());
    cmd->SetGraphicsRootSignature(rootSignature.Get());
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D12DescriptorHeap* heaps[] =
    {
        descriptors->getDescriptorHeap(),
        samplers->getHeap()
    };
    cmd->SetDescriptorHeaps(2, heaps);

    cmd->SetGraphicsRoot32BitConstants(
        0,
        sizeof(Matrix) / sizeof(UINT),
        &mvp,
        0
    );

    cmd->SetGraphicsRootConstantBufferView(
        1,
        frameAlloc.gpuAddress
    );

    cmd->SetGraphicsRootDescriptorTable(
        4,
        samplers->getGPUHandle(GraphicsSamplers::LINEAR_WRAP)
    );

    BEGIN_EVENT(cmd, "Model Viewer");

    for (const auto& meshPtr : model->getMeshes())
    {
        int materialID = meshPtr->getMaterialIndex();
        if (materialID >= 0 && materialID < (int)model->getMaterials().size())
        {
            const auto& materialPtr = model->getMaterials()[materialID];

            PerInstance perInstance;
            perInstance.modelMat = model->getModelMatrix().Transpose();
            perInstance.normalMat = getNormalMatrix().Transpose();
            perInstance.material = materialPtr->getPhong();

            cmd->SetGraphicsRootConstantBufferView(2, ringBuffer->allocateType(perInstance).gpuAddress);

            if (materialPtr->hasTexture())
            {
                cmd->SetGraphicsRootDescriptorTable(3, materialPtr->getTextureGPUHandle());
            }

            meshPtr->render(cmd);
        }
    }

    END_EVENT(cmd);

    if (showGrid && debugDrawPass)
        dd::xzSquareGrid(-10, 10, 0, 1, dd::colors::LightGray);

    if (showAxis && debugDrawPass)
        dd::axisTriad(ddConvert(Matrix::Identity), 0.1f, 1.0f);

    if (debugDrawPass)
        debugDrawPass->record(cmd, w, h, view, proj);
}

bool ModuleModelViewer::createRootSignature()
{
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    CD3DX12_ROOT_PARAMETER rootParameters[5] = {};
    CD3DX12_DESCRIPTOR_RANGE tableRanges;
    CD3DX12_DESCRIPTOR_RANGE sampRange;

    tableRanges.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, GraphicsSamplers::COUNT, 0);  // Updated

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

    if (FAILED(app->getD3D12()->getDevice()->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature))))
    {
        return false;
    }

    return true;
}

bool ModuleModelViewer::loadModel()
{
    model = std::make_unique<Model>();

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

    model->setModelMatrix(Matrix::CreateScale(0.01f));

    ModuleResources* resources = app->getResources();
    if (!resources) return false;

    const auto& materials = model->getMaterials();
    materialBuffers.resize(materials.size());

    for (size_t i = 0; i < materials.size(); ++i)
    {
        const auto& material = materials[i];
        const Material::BasicMaterial& data = material->getBasic();

        materialBuffers[i] = resources->createDefaultBuffer(
            &data,
            sizeof(Material::BasicMaterial),
            material->getName().c_str()
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

    const D3D12_INPUT_ELEMENT_DESC* inputLayout = Mesh::inputLayout;
    uint32_t inputLayoutCount = _countof(Mesh::inputLayout);

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

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = { Mesh::inputLayout, _countof(Mesh::inputLayout) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = { dataVS.data(), dataVS.size() };
    psoDesc.PS = { dataPS.data(), dataPS.size() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.NumRenderTargets = 1;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc = { 1,0 };
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

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