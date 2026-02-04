#include "Globals.h"
#include "LightingDemo.h"

#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleCamera.h"
#include "ModuleShaderDescriptors.h"
#include "GraphicsSamplers.h"  
#include "ModuleRingBuffer.h"
#include "Model.h"
#include "Mesh.h"

#include "ReadData.h"

#include "DirectXTex.h"
#include <d3d12.h>
#include "d3dx12.h"

static Matrix getNormalMatrix(const Matrix& modelMatrix)
{
    return modelMatrix.Invert().Transpose();
}

LightingDemo::LightingDemo()
{
}

LightingDemo::~LightingDemo()
{
}

bool LightingDemo::init()
{
    bool ok = createRootSignature();
    ok = ok && createPSO();
    ok = ok && loadModel();

    if (ok)
    {
        ModuleD3D12* d3d12 = app->getD3D12();

        debugDrawPass = std::make_unique<DebugDrawPass>(d3d12->getDevice(), d3d12->getDrawCommandQueue(), false);
        imguiPass = std::make_unique<ImGuiPass>(d3d12->getDevice(), d3d12->getHWnd());
    }

    return ok;
}

bool LightingDemo::cleanUp()
{
    imguiPass.reset();
    debugDrawPass.reset();
    model.reset();

    return true;
}

void LightingDemo::preRender()
{
    imguiPass->startFrame();
    ImGuizmo::BeginFrame();

    ModuleD3D12* d3d12 = app->getD3D12();

    unsigned width = d3d12->getWindowWidth();
    unsigned height = d3d12->getWindowHeight();

    ImGuizmo::SetRect(0, 0, float(width), float(height));
}

void LightingDemo::imGuiCommands()
{
    ImGui::Begin("Lighting Demo Options");
    ImGui::Separator();
    ImGui::Text("FPS: [%d]. Avg. elapsed (Ms): [%g] ", uint32_t(app->getFPS()), app->getAvgElapsedMs());
    ImGui::Separator();
    ImGui::Checkbox("Show grid", &showGrid);
    ImGui::Checkbox("Show axis", &showAxis);
    ImGui::Checkbox("Show guizmo", &showGuizmo);
    ImGui::Text("Model loaded %s with %zu meshes and %zu materials",
        model->getSrcFile().c_str(),
        model->getMeshCount(),
        model->getMaterialCount());

    size_t meshIndex = 0;
    for (const auto& mesh : model->getMeshes())
    {
        ImGui::Text("Mesh %zu with %d vertices and %d triangles",
            meshIndex++,
            mesh->getVertexCount(),
            mesh->getIndexCount() / 3);
    }

    Matrix objectMatrix = model->getModelMatrix();

    ImGui::Separator();
    static ImGuizmo::OPERATION gizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_T)) gizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) gizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_S)) gizmoOperation = ImGuizmo::SCALE;

    ImGui::RadioButton("Translate", (int*)&gizmoOperation, (int)ImGuizmo::TRANSLATE);
    ImGui::SameLine();
    ImGui::RadioButton("Rotate", (int*)&gizmoOperation, ImGuizmo::ROTATE);
    ImGui::SameLine();
    ImGui::RadioButton("Scale", (int*)&gizmoOperation, ImGuizmo::SCALE);

    float translation[3], rotation[3], scale[3];
    ImGuizmo::DecomposeMatrixToComponents((float*)&objectMatrix, translation, rotation, scale);
    bool transform_changed = ImGui::DragFloat3("Tr", translation, 0.1f);
    transform_changed = transform_changed || ImGui::DragFloat3("Rt", rotation, 0.1f);
    transform_changed = transform_changed || ImGui::DragFloat3("Sc", scale, 0.1f);

    if (transform_changed)
    {
        ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, (float*)&objectMatrix);
        model->setModelMatrix(objectMatrix);
    }

    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat3("Light Direction", reinterpret_cast<float*>(&light.L), 0.1f, -1.0f, 1.0f);
        ImGui::SameLine();
        if (ImGui::SmallButton("Normalize"))
        {
            light.L.Normalize();
        }
        ImGui::ColorEdit3("Light Colour", reinterpret_cast<float*>(&light.Lc), ImGuiColorEditFlags_NoAlpha);
        ImGui::ColorEdit3("Ambient Colour", reinterpret_cast<float*>(&light.Ac), ImGuiColorEditFlags_NoAlpha);
    }

    size_t materialIndex = 0;
    for (const auto& material : model->getMaterials())
    {
        char tmp[256];
        _snprintf_s(tmp, 255, "Material %zu: %s", materialIndex++, material->getName());

        if (ImGui::CollapsingHeader(tmp, ImGuiTreeNodeFlags_DefaultOpen))
        {
            Material::Data matData = material->getData();
            if (ImGui::ColorEdit3("Base Colour", reinterpret_cast<float*>(&matData.baseColour)))
            {
            }

            bool hasTexture = matData.hasColourTexture;
            if (ImGui::Checkbox("Use Texture", &hasTexture))
            {
                matData.hasColourTexture = hasTexture;
            }
        }
    }

    ImGui::End();

    ModuleCamera* camera = app->getCamera();
    ModuleD3D12* d3d12 = app->getD3D12();

    if (showGuizmo)
    {
        unsigned width = d3d12->getWindowWidth();
        unsigned height = d3d12->getWindowHeight();

        const Matrix& viewMatrix = camera->getView();
        Matrix projMatrix = ModuleCamera::getPerspectiveProj(float(width) / float(height));

        ImGuizmo::Manipulate((const float*)&viewMatrix, (const float*)&projMatrix, gizmoOperation, ImGuizmo::LOCAL, (float*)&objectMatrix);
    }

    ImGuiIO& io = ImGui::GetIO();

    camera->setEnable(!io.WantCaptureMouse && !ImGuizmo::IsUsing());

    if (ImGuizmo::IsUsing())
    {
        model->setModelMatrix(objectMatrix);
    }
}

void LightingDemo::render()
{
    imGuiCommands();

    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleCamera* camera = app->getCamera();
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    GraphicsSamplers* samplers = app->getGraphicsSamplers(); 
    ModuleRingBuffer* ringBuffer = app->getRingBuffer();

    ID3D12GraphicsCommandList* commandList = d3d12->getCommandList();

    commandList->Reset(d3d12->getCommandAllocator(), pso.Get());

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    commandList->ResourceBarrier(1, &barrier);

    unsigned width = d3d12->getWindowWidth();
    unsigned height = d3d12->getWindowHeight();

    const Matrix& view = camera->getView();
    Matrix proj = ModuleCamera::getPerspectiveProj(float(width) / float(height));

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

    float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d12->getRenderTargetDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = d3d12->getDepthStencilDescriptor();

    PerFrame perFrame;
    perFrame.L = light.L;
    perFrame.Lc = light.Lc;
    perFrame.Ac = light.Ac;
    perFrame.viewPos = camera->getPos();

    perFrame.L.Normalize();

    commandList->OMSetRenderTargets(1, &rtv, false, &dsv);

    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        descriptors->getHeap(),
        samplers->getHeap()  
    };
    commandList->SetDescriptorHeaps(2, descriptorHeaps);

    commandList->SetGraphicsRoot32BitConstants(0, sizeof(Matrix) / sizeof(UINT32), &mvp, 0);
    commandList->SetGraphicsRootConstantBufferView(1, ringBuffer->allocateConstantBuffer(&perFrame, sizeof(PerFrame)));
    commandList->SetGraphicsRootDescriptorTable(4, samplers->getGPUHandle(GraphicsSamplers::LINEAR_WRAP));

    BEGIN_EVENT(commandList, "Model Render Pass");

    for (const auto& mesh : model->getMeshes())
    {
        int materialIndex = mesh->getMaterialIndex();
        if (materialIndex >= 0 && materialIndex < static_cast<int>(model->getMaterialCount()))
        {
            const auto& material = model->getMaterials()[materialIndex];

            PerInstance perInstance;
            perInstance.modelMat = model->getModelMatrix().Transpose();
            perInstance.normalMat = getNormalMatrix(model->getModelMatrix()).Transpose();
            perInstance.material = material->getData();

            commandList->SetGraphicsRootConstantBufferView(2,
                ringBuffer->allocateConstantBuffer(&perInstance, sizeof(PerInstance)));

            D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = material->getTextureGPUHandle();
            commandList->SetGraphicsRootDescriptorTable(3, textureHandle);

            mesh->draw(commandList);
        }
    }

    END_EVENT(commandList);

    if (showGrid) dd::xzSquareGrid(-10.0f, 10.0f, 0.0f, 1.0f, dd::colors::LightGray);
    if (showAxis) dd::axisTriad(ddConvert(Matrix::Identity), 0.1f, 1.0f);

    debugDrawPass->record(commandList, width, height, view, proj);
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

bool LightingDemo::createRootSignature()
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

bool LightingDemo::createPSO()
{
    const D3D12_INPUT_ELEMENT_DESC* inputLayout = Mesh::getInputLayout();
    UINT inputLayoutCount = Mesh::getInputLayoutCount();

    auto dataVS = DX::ReadData(L"LightingDemoVS.cso"); 
    auto dataPS = DX::ReadData(L"LightingDemoPS.cso"); 

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

bool LightingDemo::loadModel()
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