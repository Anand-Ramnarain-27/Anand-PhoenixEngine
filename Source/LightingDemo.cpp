#include "Globals.h"
#include "LightingDemo.h"

#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleCamera.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "GraphicsSamplers.h"  // Changed from ModuleSamplers
#include "ModuleRingBuffer.h"
#include "ModuleResources.h"
#include "Model.h"            // Changed from BasicModel
#include "Mesh.h"             // Changed from BasicMesh
#include "RenderTexture.h"

#include "ReadData.h"

#include "DirectXTex.h"
#include <d3d12.h>
#include "d3dx12.h"


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
        ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();

        debugDrawPass = std::make_unique<DebugDrawPass>(d3d12->getDevice(), d3d12->getDrawCommandQueue(), false);

        descTable = descriptors->allocTable();
        imguiPass = std::make_unique<ImGuiPass>(d3d12->getDevice(), d3d12->getHWnd(), descTable.getCPUHandle(), descTable.getGPUHandle());

        renderTexture = std::make_unique<RenderTexture>("LightingDemo", DXGI_FORMAT_R8G8B8A8_UNORM,
            Vector4(0.188f, 0.208f, 0.259f, 1.0f), DXGI_FORMAT_D32_FLOAT, 1.0f);
    }

    return true;
}

bool LightingDemo::cleanUp()
{
    imguiPass.reset();
    return true;
}

void LightingDemo::preRender()
{
    imguiPass->startFrame();
    ImGuizmo::BeginFrame();

    ImGuiID dockspace_id = ImGui::GetID("MyDockNodeId");
    ImGui::DockSpaceOverViewport(dockspace_id);

    static bool init = true;
    ImVec2 mainSize = ImGui::GetMainViewport()->Size;
    if (init)
    {
        init = false;
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_CentralNode);
        ImGui::DockBuilderSetNodeSize(dockspace_id, mainSize);

        ImGuiID dock_id_left = 0, dock_id_right = 0;
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.75f, &dock_id_left, &dock_id_right);
        ImGui::DockBuilderDockWindow("Lighting Demo Options", dock_id_right);
        ImGui::DockBuilderDockWindow("Scene", dock_id_left);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    if (canvasSize.x > 0.0f && canvasSize.y > 0.0f)
        renderTexture->resize(int(canvasSize.x), int(canvasSize.y));
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
        model->getSrcFile().c_str(), model->getMeshCount(), model->getMaterialCount());

    for (const auto& mesh : model->getMeshes())
    {
        ImGui::Text("Mesh with %u vertices and %u triangles",
            mesh->getVertexCount(), mesh->getIndexCount() / 3);
    }

    Matrix objectMatrix = model->getModelMatrix();

    ImGui::Separator();
    // Set ImGuizmo operation mode (TRANSLATE, ROTATE, SCALE)
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

    for (const auto& material : model->getMaterials())
    {
        char tmp[256];
        _snprintf_s(tmp, 255, "Material %s", material->getName());

        if (ImGui::CollapsingHeader(tmp, ImGuiTreeNodeFlags_DefaultOpen))
        {
            Material::Data matData = material->getData();

            // Convert to XMFLOAT3 for ColorEdit3
            XMFLOAT3 diffuseColour = XMFLOAT3(matData.baseColour.x, matData.baseColour.y, matData.baseColour.z);

            if (ImGui::ColorEdit3("Diffuse Colour", reinterpret_cast<float*>(&diffuseColour)))
            {
                matData.baseColour.x = diffuseColour.x;
                matData.baseColour.y = diffuseColour.y;
                matData.baseColour.z = diffuseColour.z;
                // Note: Need to add a setData method to Material class
                // material->setData(matData);
            }

            bool hasTexture = matData.hasColourTexture;
            if (ImGui::Checkbox("Use Texture", &hasTexture))
            {
                matData.hasColourTexture = hasTexture;
                // material->setData(matData);
            }

            // For PBRPhong materials
            PBRPhongMaterialData pbrData = material->getPBRPhongData();
            if (ImGui::ColorEdit3("Specular Colour", reinterpret_cast<float*>(&pbrData.specularColour)))
            {
                material->setPBRPhongData(pbrData);
            }

            if (ImGui::DragFloat("shininess", &pbrData.shininess))
            {
                material->setPBRPhongData(pbrData);
            }
        }
    }

    ImGui::End();

    ModuleCamera* camera = app->getCamera();
    bool viewerFocused = false;
    ImGui::Begin("Scene");
    const char* frameName = "Scene Frame";
    ImGuiID id(10);

    ImVec2 max = ImGui::GetWindowContentRegionMax();
    ImVec2 min = ImGui::GetWindowContentRegionMin();
    canvasPos = min;
    canvasSize = ImVec2(max.x - min.x, max.y - min.y);
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    ImGui::BeginChildFrame(id, canvasSize, ImGuiWindowFlags_NoScrollbar);
    viewerFocused = ImGui::IsWindowFocused();

    if (renderTexture->getSrvTableDesc())
    {
        ImGui::Image((ImTextureID)renderTexture->getSrvHandle().ptr, canvasSize);
    }

    if (showGuizmo)
    {
        const Matrix& viewMatrix = camera->getView();
        Matrix projMatrix = ModuleCamera::getPerspectiveProj(float(canvasSize.x) / float(canvasSize.y));

        // Manipulate the object
        ImGuizmo::SetRect(cursorPos.x, cursorPos.y, canvasSize.x, canvasSize.y);
        ImGuizmo::SetDrawlist();
        ImGuizmo::Manipulate((const float*)&viewMatrix, (const float*)&projMatrix,
            gizmoOperation, ImGuizmo::LOCAL, (float*)&objectMatrix);
    }

    ImGui::EndChildFrame();
    ImGui::End();

    camera->setEnable(viewerFocused && !ImGuizmo::IsUsing());

    if (ImGuizmo::IsUsing())
    {
        model->setModelMatrix(objectMatrix);
    }
}

void LightingDemo::renderToTexture(ID3D12GraphicsCommandList* commandList)
{
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleCamera* camera = app->getCamera();
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    ModuleRTDescriptors* rtDescriptors = app->getRTDescriptors();
    ModuleDSDescriptors* dsDescriptors = app->getDSDescriptors();
    GraphicsSamplers* samplers = app->getGraphicsSamplers();  // Changed from ModuleSamplers
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

    renderTexture->beginRender(commandList);

    commandList->SetGraphicsRoot32BitConstants(0, sizeof(Matrix) / sizeof(UINT32), &mvp, 0);
    commandList->SetGraphicsRootConstantBufferView(1, ringBuffer->allocate(&perFrame));
    commandList->SetGraphicsRootDescriptorTable(4, samplers->getGPUHandle(GraphicsSamplers::LINEAR_WRAP));

    BEGIN_EVENT(commandList, "Model Render Pass");

    const auto& materials = model->getMaterials();
    size_t materialCount = materials.size();

    for (const auto& mesh : model->getMeshes())
    {
        int materialIndex = mesh->getMaterialIndex();
        if (materialIndex >= 0 && materialIndex < static_cast<int>(model->getMaterialCount()))
        {
            const auto& material = model->getMaterials()[materialIndex];

            // Calculate normal matrix (inverse transpose of model matrix)
            Matrix normalMatrix = model->getModelMatrix();
            normalMatrix.Invert();
            normalMatrix.Transpose();

            PerInstance perInstance;
            perInstance.modelMat = model->getModelMatrix().Transpose();
            perInstance.normalMat = normalMatrix.Transpose();
            perInstance.material = material->getData();

            commandList->SetGraphicsRootConstantBufferView(2, ringBuffer->allocate(&perInstance));
            commandList->SetGraphicsRootDescriptorTable(3, material->getDescriptorTable().getGPUHandle());

            mesh->draw(commandList);
        }
    }

    END_EVENT(commandList);

    if (showGrid) dd::xzSquareGrid(-10.0f, 10.0f, 0.0f, 1.0f, dd::colors::LightGray);
    if (showAxis) dd::axisTriad(ddConvert(Matrix::Identity), 0.1f, 1.0f);

    debugDrawPass->record(commandList, width, height, view, proj);

    renderTexture->endRender(commandList);
}

void LightingDemo::render()
{
    imGuiCommands();

    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    GraphicsSamplers* samplers = app->getGraphicsSamplers();  // Changed from ModuleSamplers

    ID3D12GraphicsCommandList* commandList = d3d12->getCommandList();

    commandList->Reset(d3d12->getCommandAllocator(), nullptr);

    commandList->SetGraphicsRootSignature(rootSignature.Get());
    ID3D12DescriptorHeap* descriptorHeaps[] = { descriptors->getHeap(), samplers->getHeap() };
    commandList->SetDescriptorHeaps(2, descriptorHeaps);

    if (renderTexture->isValid() && canvasSize.x > 0.0f && canvasSize.y > 0.0f)
    {
        renderToTexture(commandList);
    }

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        d3d12->getBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d12->getRenderTargetDescriptor();
    commandList->OMSetRenderTargets(1, &rtv, false, nullptr);

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    imguiPass->record(commandList);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
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

    if (FAILED(app->getD3D12()->getDevice()->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature))))
    {
        return false;
    }

    return true;
}

bool LightingDemo::createPSO()
{
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    auto dataVS = DX::ReadData(L"LightingDemoVS.cso");
    auto dataPS = DX::ReadData(L"LightingDemoPS.cso");

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
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.NumRenderTargets = 1;

    return SUCCEEDED(app->getD3D12()->getDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
}

bool LightingDemo::loadModel()
{
    model = std::make_unique<Model>();
    // Use loadPBRPhong instead of load to support PBR materials
    if (!model->loadPBRPhong("Assets/Models/Duck/duck.gltf", "Assets/Models/Duck/"))
    {
        // Fallback to regular load if PBR fails
        if (!model->load("Assets/Models/Duck/duck.gltf", "Assets/Models/Duck/"))
        {
            return false;
        }
    }
    model->setModelMatrix(Matrix::CreateScale(0.01f, 0.01f, 0.01f));
    return true;
}