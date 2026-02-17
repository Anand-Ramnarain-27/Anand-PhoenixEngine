#include "Globals.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleAssets.h"
#include "RenderTexture.h"
#include "DebugDrawPass.h"
#include "ImGuiPass.h"
#include "EmptyScene.h"
#include "ModuleScene.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "LightCollector.h"
#include "ComponentDirectionalLight.h"
#include "ComponentPointLight.h"
#include "ComponentSpotLight.h"
#include "ComponentCamera.h"
#include "ComponentFactory.h"
#include "ModuleCamera.h"
#include "PrefabManager.h"

#include <d3dx12.h>
#include <filesystem>

ModuleEditor::ModuleEditor() = default;
ModuleEditor::~ModuleEditor() = default;

bool ModuleEditor::init()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();

    descTable = descriptors->allocTable();
    imguiPass = std::make_unique<ImGuiPass>(d3d12->getDevice(), d3d12->getHWnd(), descTable.getCPUHandle(), descTable.getGPUHandle());
    debugDrawPass = std::make_unique<DebugDrawPass>(d3d12->getDevice(), d3d12->getDrawCommandQueue(), false);
    viewportRT = std::make_unique<RenderTexture>("EditorViewport", DXGI_FORMAT_R8G8B8A8_UNORM, Vector4(0.1f, 0.1f, 0.1f, 1.0f), DXGI_FORMAT_D32_FLOAT, 1.0f);
    sceneManager = std::make_unique<SceneManager>();
    meshPipeline = std::make_unique<MeshPipeline>();

    if (!meshPipeline->init(d3d12->getDevice()))
        return false;

    sceneManager->setScene(std::make_unique<EmptyScene>(), d3d12->getDevice());
    log("[Editor] Initialized", ImVec4(0.6f, 1.0f, 0.6f, 1.0f));

    // GPU timestamp query heap
    D3D12_QUERY_HEAP_DESC qDesc = {};
    qDesc.Count = 2;
    qDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    d3d12->getDevice()->CreateQueryHeap(&qDesc, IID_PPV_ARGS(&gpuQueryHeap));

    auto readbackHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    auto readbackDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * 2);
    d3d12->getDevice()->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE,
        &readbackDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&gpuReadbackBuffer));

    {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer((sizeof(CameraConstants) + 255) & ~255);
        d3d12->getDevice()->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cameraConstantBuffer));
        cameraConstantBuffer->SetName(L"CameraCB");
    }

    {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer((sizeof(ObjectConstants) + 255) & ~255);
        d3d12->getDevice()->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&objectConstantBuffer));
        objectConstantBuffer->SetName(L"ObjectCB");
    }

    {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(
            (sizeof(MeshPipeline::LightCB) + 255) & ~255);
        d3d12->getDevice()->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&lightConstantBuffer));
        lightConstantBuffer->SetName(L"LightCB");
    }

    m_saveDialog.setExtensionFilter(".json");
    m_loadDialog.setExtensionFilter(".json");

    return true;
}

bool ModuleEditor::cleanUp()
{
    imguiPass.reset();
    debugDrawPass.reset();
    viewportRT.reset();
    sceneManager.reset();
    gpuQueryHeap.Reset();
    gpuReadbackBuffer.Reset();
    return true;
}

void ModuleEditor::preRender()
{
    ModuleD3D12* d3d12 = app->getD3D12();

    if (pendingViewportResize)
    {
        if (pendingViewportWidth > 4 && pendingViewportHeight > 4)
        {
            d3d12->flush();
            viewportRT->resize(pendingViewportWidth, pendingViewportHeight);
            if (sceneManager)
                sceneManager->onViewportResized(pendingViewportWidth, pendingViewportHeight);
        }
        pendingViewportResize = false;
    }

    imguiPass->startFrame();
    ImGuizmo::BeginFrame();

    if (sceneManager)
        sceneManager->update(app->getElapsedMilis() * 0.001f);

    updateFPS();

    drawDockspace();
    drawMenuBar();

    if (showViewport) { drawViewport(); drawGizmoToolbar(); drawViewportOverlay(); }
    if (showHierarchy)    drawHierarchy();
    if (showInspector)    drawInspector();
    if (showConsole)      drawConsole();
    if (showPerformance)  drawPerformanceWindow();
    if (showAssetBrowser) drawAssetBrowser();
    if (showSceneSettings)drawSceneSettings();

    if (m_saveDialog.draw())
    {
        std::string path = m_saveDialog.getSelectedPath();
        if (sceneManager && sceneManager->getActiveScene())
        {
            if (sceneManager->saveCurrentScene(path))
                log("Scene saved!", ImVec4(0.6f, 1.0f, 0.6f, 1.0f));
            else
                log("Failed to save scene.", ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        }
    }

    if (m_loadDialog.draw())
    {
        std::string path = m_loadDialog.getSelectedPath();
        if (sceneManager && sceneManager->getActiveScene())
        {
            if (sceneManager->loadScene(path))
                log("Scene loaded!", ImVec4(0.6f, 1.0f, 0.6f, 1.0f));
            else
                log("Failed to load scene.", ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        }
    }

    if (viewportRT && viewportSize.x > 4 && viewportSize.y > 4)
    {
        if (viewportSize.x != lastViewportSize.x || viewportSize.y != lastViewportSize.y)
        {
            pendingViewportResize = true;
            pendingViewportWidth = (UINT)viewportSize.x;
            pendingViewportHeight = (UINT)viewportSize.y;
            lastViewportSize = viewportSize;
        }
    }
}

void ModuleEditor::render()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();
    ID3D12GraphicsCommandList* cmd = d3d12->getCommandList();

    cmd->Reset(d3d12->getCommandAllocator(), nullptr);
    cmd->EndQuery(gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);

    ID3D12DescriptorHeap* heaps[] = { descriptors->getHeap() };
    cmd->SetDescriptorHeaps(1, heaps);

    if (showNewSceneConfirmation)
    {
        ImGui::OpenPopup("New Scene?");
        showNewSceneConfirmation = false;
    }
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("New Scene?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("This will clear the current scene.");
        ImGui::TextColored(ImVec4(1, 0.6f, 0.4f, 1), "Unsaved changes will be lost!");
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        if (ImGui::Button("Create New Scene", ImVec2(160, 0)))
        {
            app->getD3D12()->flush();
            sceneManager->setScene(std::make_unique<EmptyScene>(), app->getD3D12()->getDevice());
            selectedGameObject = nullptr;
            log("New scene created.", ImVec4(0.6f, 1.0f, 0.6f, 1.0f));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (viewportRT && viewportRT->isValid())
        renderViewportToTexture(cmd);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ResourceBarrier(1, &barrier);

    auto rtv = d3d12->getRenderTargetDescriptor();
    cmd->OMSetRenderTargets(1, &rtv, false, nullptr);
    float clear[] = { 0, 0, 0, 1 };
    cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);

    imguiPass->record(cmd);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    cmd->ResourceBarrier(1, &barrier);

    cmd->EndQuery(gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
    cmd->ResolveQueryData(gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
        0, 2, gpuReadbackBuffer.Get(), 0);
    cmd->Close();

    ID3D12CommandList* lists[] = { cmd };
    d3d12->getDrawCommandQueue()->ExecuteCommandLists(1, lists);

    UINT64* data = nullptr;
    if (SUCCEEDED(gpuReadbackBuffer->Map(0, nullptr, (void**)&data)) && data)
    {
        UINT64 freq = 0;
        d3d12->getDrawCommandQueue()->GetTimestampFrequency(&freq);
        gpuFrameTimeMs = double(data[1] - data[0]) / double(freq) * 1000.0;
        gpuTimerReady = true;
        gpuReadbackBuffer->Unmap(0, nullptr);
    }
}

void ModuleEditor::renderViewportToTexture(ID3D12GraphicsCommandList* cmd)
{
    ModuleCamera* camera = app->getCamera();
    const UINT width = UINT(viewportSize.x);
    const UINT height = UINT(viewportSize.y);
    if (!viewportRT || width == 0 || height == 0) return;

    const Matrix& view = camera->getView();
    Matrix proj = ModuleCamera::getPerspectiveProj(float(width) / float(height));

    viewportRT->beginRender(cmd);
    BEGIN_EVENT(cmd, "Editor Viewport");

    cmd->SetPipelineState(meshPipeline->getPSO());
    cmd->SetGraphicsRootSignature(meshPipeline->getRootSig());

    const EditorSceneSettings& s = sceneManager->getSettings();
    MeshPipeline::LightCB lightData = {};

    Matrix vp = (view * proj).Transpose();
    cmd->SetGraphicsRoot32BitConstants(0, 16, &vp, 0);

    Matrix identity = Matrix::Identity.Transpose();
    cmd->SetGraphicsRoot32BitConstants(1, 16, &identity, 0);

    if (sceneManager)
        sceneManager->render(cmd, *camera, width, height);

    if (s.showGrid)
        dd::xzSquareGrid(-100.0f, 100.0f, 0.0f, 1.0f, dd::colors::Gray);
    if (s.showAxis)
    {
        Matrix id = Matrix::Identity;
        dd::axisTriad(id.m[0], 0.0f, 2.0f, 2.0f);
    }

    debugDrawPass->record(cmd, width, height, view, proj);
    END_EVENT(cmd);
    viewportRT->endRender(cmd);
}

void ModuleEditor::drawDockspace()
{
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##MainDockspace", nullptr, flags);
    ImGui::PopStyleVar(2);

    ImGuiID dockID = ImGui::GetID("Dockspace");
    ImGui::DockSpace(dockID, ImVec2(0, 0));

    if (firstFrame)
    {
        firstFrame = false;
        ImGui::DockBuilderRemoveNode(dockID);
        ImGui::DockBuilderAddNode(dockID, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockID, vp->Size);

        ImGuiID left, right, bottom, center, rightPanel;
        ImGui::DockBuilderSplitNode(dockID, ImGuiDir_Left, 0.20f, &left, &right);
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.25f, &bottom, &right);
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Right, 0.28f, &rightPanel, &center);

        ImGui::DockBuilderDockWindow("Hierarchy", left);
        ImGui::DockBuilderDockWindow("Inspector", rightPanel);
        ImGui::DockBuilderDockWindow("Scene Settings", rightPanel);
        ImGui::DockBuilderDockWindow("Viewport", center);
        ImGui::DockBuilderDockWindow("Console", bottom);
        ImGui::DockBuilderDockWindow("Performance", bottom);
        ImGui::DockBuilderDockWindow("Asset Browser", bottom);
        ImGui::DockBuilderFinish(dockID);
    }

    ImGui::End();
}

void ModuleEditor::drawMenuBar()
{
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New Scene", "Ctrl+N"))
            showNewSceneConfirmation = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
            m_saveDialog.open(FileDialog::Type::Save, "Save Scene", "Library/Scenes");
        if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
            m_saveDialog.open(FileDialog::Type::Save, "Save Scene", "Library/Scenes/");
        ImGui::Separator();
        if (ImGui::MenuItem("Load Scene...", "Ctrl+O"))
            m_loadDialog.open(FileDialog::Type::Open, "Load Scene", "Library/Scenes/");
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")) { /* signal quit */ }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Create Empty GameObject", "Ctrl+Shift+N"))
            createEmptyGameObject();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy);
        ImGui::MenuItem("Inspector", nullptr, &showInspector);
        ImGui::MenuItem("Console", nullptr, &showConsole);
        ImGui::MenuItem("Viewport", nullptr, &showViewport);
        ImGui::MenuItem("Performance", nullptr, &showPerformance);
        ImGui::MenuItem("Asset Browser", nullptr, &showAssetBrowser);
        ImGui::MenuItem("Scene Settings", nullptr, &showSceneSettings);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("GameObject"))
    {
        if (ImGui::MenuItem("Create Empty"))
            createEmptyGameObject();
        if (ImGui::MenuItem("Create Empty Child") && selectedGameObject)
            createEmptyGameObject("Empty", selectedGameObject);
        ImGui::Separator();
        if (ImGui::MenuItem("Add Camera Component") && selectedGameObject)
        {
            if (!selectedGameObject->getComponent<ComponentCamera>())
            {
                auto cam = ComponentFactory::CreateComponent(Component::Type::Camera,
                    selectedGameObject);
                selectedGameObject->addComponent(std::move(cam));
            }
        }
        if (ImGui::MenuItem("Add Mesh Component") && selectedGameObject)
        {
            if (!selectedGameObject->getComponent<ComponentMesh>())
            {
                auto mesh = ComponentFactory::CreateComponent(Component::Type::Mesh,
                    selectedGameObject);
                selectedGameObject->addComponent(std::move(mesh));
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene"))
    {
        if (ImGui::MenuItem("Play", nullptr, false,
            sceneManager && !sceneManager->isPlaying()))
            sceneManager->play();
        if (ImGui::MenuItem("Pause", nullptr, false,
            sceneManager && sceneManager->isPlaying()))
            sceneManager->pause();
        if (ImGui::MenuItem("Stop", nullptr, false,
            sceneManager && sceneManager->getState() != SceneManager::PlayState::Stopped))
            sceneManager->stop();
        ImGui::EndMenu();
    }

    {
        float barW = ImGui::GetContentRegionAvail().x;
        float btnW = 70.0f;
        float totalW = btnW * 3 + ImGui::GetStyle().ItemSpacing.x * 2;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (barW - totalW) * 0.5f);

        bool playing = sceneManager && sceneManager->isPlaying();
        bool paused = sceneManager && sceneManager->getState() == SceneManager::PlayState::Paused;

        if (playing) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1));
        if (ImGui::Button("  Play  ", ImVec2(btnW, 0)) && sceneManager && !playing)
            sceneManager->play();
        if (playing) ImGui::PopStyleColor();

        ImGui::SameLine();
        if (paused) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.7f, 0.1f, 1));
        if (ImGui::Button(" Pause  ", ImVec2(btnW, 0)) && sceneManager && playing)
            sceneManager->pause();
        if (paused) ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button("  Stop  ", ImVec2(btnW, 0)) && sceneManager)
            sceneManager->stop();
    }

    ImGui::EndMainMenuBar();
}

void ModuleEditor::drawGizmoToolbar()
{
    if (!ImGui::GetIO().WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_T)) gizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) gizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_S)) gizmoOperation = ImGuizmo::SCALE;
        if (ImGui::IsKeyPressed(ImGuiKey_G)) gizmoMode = (gizmoMode == ImGuizmo::LOCAL)
            ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
    }

    ImGuiWindow* win = ImGui::FindWindowByName("Viewport");
    if (!win) return;

    ImVec2 pos = { win->Pos.x + 8, win->Pos.y + 28 };
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.7f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing;

    if (!ImGui::Begin("##GizmoToolbar", nullptr, flags))
    {
        ImGui::End(); return;
    }

    auto gizmoBtn = [&](const char* label, ImGuizmo::OPERATION op)
        {
            bool active = (gizmoOperation == op);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1));
            if (ImGui::Button(label, ImVec2(40, 22))) gizmoOperation = op;
            if (active) ImGui::PopStyleColor();
            ImGui::SameLine(0, 2);
        };

    gizmoBtn("T", ImGuizmo::TRANSLATE);
    gizmoBtn("R", ImGuizmo::ROTATE);
    gizmoBtn("S", ImGuizmo::SCALE);

    ImGui::SameLine(0, 8);
    bool local = (gizmoMode == ImGuizmo::LOCAL);
    if (ImGui::Button(local ? "Local" : "World", ImVec2(48, 22)))
        gizmoMode = local ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

    ImGui::SameLine(0, 8);
    ImGui::Checkbox("Snap", &useSnap);

    if (useSnap)
    {
        ImGui::SameLine(0, 4);
        if (gizmoOperation == ImGuizmo::TRANSLATE)
            ImGui::TextDisabled("%.2f", snapTranslate[0]);
        else if (gizmoOperation == ImGuizmo::ROTATE)
            ImGui::TextDisabled("%.0f°", snapRotate);
        else
            ImGui::TextDisabled("%.2f", snapScale);
    }

    ImGui::End();
}

void ModuleEditor::drawHierarchy()
{
    ImGui::Begin("Hierarchy", &showHierarchy);

    IScene* active = sceneManager ? sceneManager->getActiveScene() : nullptr;
    ModuleScene* scene = active ? active->getModuleScene() : nullptr;

    if (!scene) { ImGui::End(); return; }

    if (ImGui::Button("+ Empty")) createEmptyGameObject();
    ImGui::SameLine();
    if (ImGui::Button("+ Child") && selectedGameObject)
        createEmptyGameObject("Empty", selectedGameObject);

    ImGui::Separator();

    drawHierarchyNode(scene->getRoot());

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)
        && !ImGui::IsAnyItemHovered())
        ImGui::OpenPopup("##HierarchyBlank");

    hierarchyBlankContextMenu();

    ImGui::End();
}

void ModuleEditor::drawHierarchyNode(GameObject* go)
{
    if (!go) return;

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (go == selectedGameObject) flags |= ImGuiTreeNodeFlags_Selected;
    if (go->getChildren().empty()) flags |= ImGuiTreeNodeFlags_Leaf;

    if (renamingObject && renamingTarget == go)
    {
        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##rename", renameBuffer, sizeof(renameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
        {
            go->setName(renameBuffer);
            renamingObject = false;
            renamingTarget = nullptr;
        }
        if (ImGui::IsItemDeactivated())
        {
            go->setName(renameBuffer);
            renamingObject = false;
            renamingTarget = nullptr;
        }

        for (auto* child : go->getChildren())
            drawHierarchyNode(child);
        return;
    }

    bool open = ImGui::TreeNodeEx((void*)(uintptr_t)go->getUID(), flags,
        go->getName().c_str());

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        selectedGameObject = go;

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
    {
        renamingObject = true;
        renamingTarget = go;
        strncpy_s(renameBuffer, go->getName().c_str(), sizeof(renameBuffer) - 1);
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        selectedGameObject = go;
    if (ImGui::BeginPopupContextItem())
    {
        hierarchyItemContextMenu(go);
        ImGui::EndPopup();
    }

    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("GO_PTR", &go, sizeof(GameObject*));
        ImGui::Text("Move: %s", go->getName().c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("GO_PTR"))
        {
            GameObject* dragged = *(GameObject**)p->Data;
            if (dragged && dragged != go)
                dragged->setParent(go);
        }
        ImGui::EndDragDropTarget();
    }

    if (open)
    {
        for (auto* child : go->getChildren())
            drawHierarchyNode(child);
        ImGui::TreePop();
    }
}

void ModuleEditor::hierarchyItemContextMenu(GameObject* go)
{
    ImGui::TextDisabled("%s", go->getName().c_str());
    ImGui::Separator();

    if (ImGui::MenuItem("Rename"))
    {
        renamingObject = true;
        renamingTarget = go;
        strncpy_s(renameBuffer, go->getName().c_str(), sizeof(renameBuffer) - 1);
    }

    if (ImGui::MenuItem("Duplicate"))
        createEmptyGameObject((go->getName() + " (Copy)").c_str(), go->getParent());

    ImGui::Separator();

    if(ImGui::BeginMenu("Add Component"))
    {
        if (ImGui::MenuItem("Camera") && !go->getComponent<ComponentCamera>())
        {
            go->addComponent(
                ComponentFactory::CreateComponent(Component::Type::Camera, go));
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Mesh") && !go->getComponent<ComponentMesh>())
        {
            go->addComponent(
                ComponentFactory::CreateComponent(Component::Type::Mesh, go));
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndMenu();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Create Empty Child"))
        createEmptyGameObject("Empty", go);

    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    if (ImGui::MenuItem("Delete"))
        deleteGameObject(go);
    ImGui::PopStyleColor();
}

void ModuleEditor::hierarchyBlankContextMenu()
{
    if (ImGui::BeginPopup("##HierarchyBlank"))
    {
        if (ImGui::MenuItem("Create Empty GameObject"))
            createEmptyGameObject();
        if (ImGui::MenuItem("Create Empty Child") && selectedGameObject)
            createEmptyGameObject("Empty", selectedGameObject);
        ImGui::EndPopup();
    }
}

void ModuleEditor::drawInspector()
{
    ImGui::Begin("Inspector", &showInspector);

    if (!selectedGameObject)
    {
        ImGui::TextDisabled("No GameObject selected.");
        ImGui::End();
        return;
    }

    bool active = selectedGameObject->isActive();
    if (ImGui::Checkbox("##active", &active))
        selectedGameObject->setActive(active);
    ImGui::SameLine();

    char nameBuf[256];
    strncpy_s(nameBuf, selectedGameObject->getName().c_str(), sizeof(nameBuf) - 1);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##goname", nameBuf, sizeof(nameBuf)))
        selectedGameObject->setName(nameBuf);

    ImGui::TextDisabled("UID: %u", selectedGameObject->getUID());
    ImGui::Separator();

    if (ComponentTransform* t = selectedGameObject->getTransform())
    {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            float pos[3] = { t->position.x, t->position.y, t->position.z };
            if (ImGui::DragFloat3("Position", pos, 0.1f))
            {
                t->position = Vector3(pos[0], pos[1], pos[2]); t->markDirty();
            }

            Vector3 euler = t->rotation.ToEuler();
            float deg[3] = { euler.x * 57.2957795f,
                                euler.y * 57.2957795f,
                                euler.z * 57.2957795f };
            if (ImGui::DragFloat3("Rotation", deg, 0.5f))
            {
                t->rotation = Quaternion::CreateFromYawPitchRoll(
                    deg[1] * 0.0174532925f,
                    deg[0] * 0.0174532925f,
                    deg[2] * 0.0174532925f);
                t->markDirty();
            }

            float scl[3] = { t->scale.x, t->scale.y, t->scale.z };
            if (ImGui::DragFloat3("Scale", scl, 0.01f))
            {
                t->scale = Vector3(scl[0], scl[1], scl[2]); t->markDirty();
            }
        }
    }

    Component::Type toRemove = Component::Type::Transform; 
    bool            wantsRemove = false;

    for (const auto& comp : selectedGameObject->getComponents())
    {
        if (comp->getType() == Component::Type::Transform)
            continue;

        ImGui::PushID((int)comp->getType());

        bool headerOpen = true;  
        const char* label = (comp->getType() == Component::Type::Camera) ? "Camera"
            : (comp->getType() == Component::Type::Mesh) ? "Mesh"
            : "Component";

        ImGuiTreeNodeFlags hFlags = ImGuiTreeNodeFlags_DefaultOpen
            | ImGuiTreeNodeFlags_AllowItemOverlap
            | ImGuiTreeNodeFlags_ClipLabelForTrailingButton;
        bool open = ImGui::CollapsingHeader(label, &headerOpen, hFlags);

        if (!headerOpen)
        {
            toRemove = comp->getType();
            wantsRemove = true;
        }

        if (open)
        {
            if (comp->getType() == Component::Type::Camera)
                drawComponentCamera(static_cast<ComponentCamera*>(comp.get()));
            else if (comp->getType() == Component::Type::Mesh)
                drawComponentMesh(static_cast<ComponentMesh*>(comp.get()));
            else
                comp->onEditor();
        }

        if (ImGui::BeginPopupContextItem("##compctx"))
        {
            ImGui::TextDisabled("%s", label);
            ImGui::Separator();
            if (ImGui::MenuItem("Remove Component"))
            {
                toRemove = comp->getType();
                wantsRemove = true;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    if (wantsRemove && toRemove != Component::Type::Transform)
        selectedGameObject->removeComponentByType(toRemove);

    ImGui::Spacing();
    ImGui::Separator();
    float btnW = 180.0f;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - btnW) * 0.5f + ImGui::GetCursorPosX());
    if (ImGui::Button("Add Component", ImVec2(btnW, 0)))
        ImGui::OpenPopup("##AddComp");

    if (ImGui::BeginPopup("##AddComp"))
    {
        bool hasCamera = selectedGameObject->getComponent<ComponentCamera>() != nullptr;
        bool hasMesh = selectedGameObject->getComponent<ComponentMesh>() != nullptr;

        if (ImGui::MenuItem("Camera", nullptr, false, !hasCamera))
        {
            selectedGameObject->addComponent(
                ComponentFactory::CreateComponent(Component::Type::Camera,
                    selectedGameObject));

            ImGui::CloseCurrentPopup(); 
        }

        if (ImGui::MenuItem("Mesh", nullptr, false, !hasMesh))
        {
            selectedGameObject->addComponent(
                ComponentFactory::CreateComponent(Component::Type::Mesh,
                    selectedGameObject));

            ImGui::CloseCurrentPopup();  
        }

        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Prefab");
    static char prefabBuf[256] = "";
    ImGui::SetNextItemWidth(-80);
    ImGui::InputText("##prefabname", prefabBuf, sizeof(prefabBuf));
    ImGui::SameLine();
    if (ImGui::Button("Save") && strlen(prefabBuf) > 0)
    {
        if (PrefabManager::createPrefab(selectedGameObject, prefabBuf))
        {
            log(("Prefab saved: " + std::string(prefabBuf)).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
            prefabBuf[0] = '\0';
        }
    }

    ImGui::End();
}

void ModuleEditor::drawComponentCamera(ComponentCamera* cam)
{
    bool isMain = cam->isMainCamera();
    if (ImGui::Checkbox("Main Camera", &isMain))
        cam->setMainCamera(isMain);

    float fovDeg = cam->getFOV() * 57.2957795f;
    if (ImGui::SliderFloat("FOV", &fovDeg, 10.0f, 170.0f))
        cam->setFOV(fovDeg * 0.0174532925f);

    float nearP = cam->getNearPlane();
    float farP = cam->getFarPlane();
    if (ImGui::DragFloat("Near Plane", &nearP, 0.01f, 0.001f, farP - 0.01f))
        cam->setNearPlane(nearP);
    if (ImGui::DragFloat("Far Plane", &farP, 1.0f, nearP + 0.01f, 10000.0f))
        cam->setFarPlane(farP);

    Vector4 bg = cam->getBackgroundColor();
    if (ImGui::ColorEdit4("Background", &bg.x))
        cam->setBackgroundColor(bg);
}

void ModuleEditor::drawComponentMesh(ComponentMesh* mesh)
{
    const Model* model = mesh->getModel();
    if (model)
        ImGui::TextDisabled("Model loaded");
    else
        ImGui::TextColored(ImVec4(1, 0.6f, 0.4f, 1), "No model loaded");

    static char meshPathBuf[256] = "";
    ImGui::SetNextItemWidth(-80);
    ImGui::InputText("##meshpath", meshPathBuf, sizeof(meshPathBuf));
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        if (strlen(meshPathBuf) > 0)
        {
            if (mesh->loadModel(meshPathBuf))
                log(("Loaded: " + std::string(meshPathBuf)).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
            else
                log(("Failed to load: " + std::string(meshPathBuf)).c_str(), ImVec4(1, 0.4f, 0.4f, 1));
        }
    }
}

void ModuleEditor::drawGizmo()
{
    if (!selectedGameObject) return;

    ComponentTransform* t = selectedGameObject->getTransform();
    if (!t) return;

    ModuleCamera* camera = app->getCamera();
    if (!camera) return;

    const float w = viewportSize.x;
    const float h = viewportSize.y;
    if (w <= 0.0f || h <= 0.0f) return;

    const Matrix& view = camera->getView();
    Matrix proj = ModuleCamera::getPerspectiveProj(w / h);
    Matrix world = t->getGlobalMatrix();

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewportPos.x, viewportPos.y, w, h);

    float* snapPtr = nullptr;
    float  snapVals[3] = { 0.f, 0.f, 0.f };
    if (useSnap)
    {
        if (gizmoOperation == ImGuizmo::TRANSLATE) { snapVals[0] = snapTranslate[0]; snapVals[1] = snapTranslate[1]; snapVals[2] = snapTranslate[2]; }
        else if (gizmoOperation == ImGuizmo::ROTATE) { snapVals[0] = snapRotate; }
        else { snapVals[0] = snapScale; }
        snapPtr = snapVals;
    }

    bool manipulated = ImGuizmo::Manipulate(
        reinterpret_cast<const float*>(&view),
        reinterpret_cast<const float*>(&proj),
        gizmoOperation,
        gizmoMode,
        reinterpret_cast<float*>(&world),
        nullptr,
        snapPtr);

    if (manipulated)
    {
        Matrix localWorld = world;
        if (GameObject* par = selectedGameObject->getParent())
        {
            Matrix parentWorld = par->getTransform()->getGlobalMatrix();
            localWorld = world * parentWorld.Invert();
        }

        float translation[3], eulerDeg[3], scale[3];
        ImGuizmo::DecomposeMatrixToComponents(
            reinterpret_cast<const float*>(&localWorld),
            translation, eulerDeg, scale);

        t->position = Vector3(translation[0], translation[1], translation[2]);
        t->scale = Vector3(scale[0], scale[1], scale[2]);
        t->rotation = Quaternion::CreateFromYawPitchRoll(
            eulerDeg[1] * 0.0174532925f,
            eulerDeg[0] * 0.0174532925f,
            eulerDeg[2] * 0.0174532925f);

        t->markDirty();
    }
}

void ModuleEditor::drawViewport()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("Viewport", &showViewport,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (open)
    {
        viewportSize = ImGui::GetContentRegionAvail();

        if (viewportRT && viewportRT->isValid() && !pendingViewportResize)
        {
            ImGui::Image((ImTextureID)viewportRT->getSrvHandle().ptr, viewportSize);

            viewportPos = ImGui::GetItemRectMin();

            drawGizmo();
        }
        else
        {
            ImGui::TextDisabled("Viewport not ready...");
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void ModuleEditor::drawViewportOverlay()
{
    ImGuiWindow* win = ImGui::FindWindowByName("Viewport");
    if (!win) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 p = { win->Pos.x + 10, win->Pos.y + win->Size.y - 24 };

    char buf[160];
    sprintf_s(buf, "FPS: %.1f  CPU: %.2f ms  GPU: %.2f ms",
        app->getFPS(), app->getAvgElapsedMs(), gpuFrameTimeMs);
    dl->AddText(p, IM_COL32(0, 230, 0, 220), buf);
}

void ModuleEditor::drawConsole()
{
    ImGui::Begin("Console", &showConsole);

    if (ImGui::Button("Clear")) console.clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScrollConsole);
    ImGui::Separator();

    ImGui::BeginChild("##ConsoleScroll");
    for (const auto& e : console)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, e.color);
        ImGui::TextUnformatted(e.text.c_str());
        ImGui::PopStyleColor();
    }
    if (autoScrollConsole && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::End();
}

void ModuleEditor::drawPerformanceWindow()
{
    ImGui::Begin("Performance", &showPerformance);
    ImGui::Text("FPS:  %.1f", app->getFPS());
    ImGui::Text("CPU:  %.2f ms", app->getAvgElapsedMs());
    if (gpuTimerReady)
        ImGui::Text("GPU:  %.2f ms", gpuFrameTimeMs);
    ImGui::Separator();
    ImGui::Text("VRAM: %llu MB", gpuMemoryMB);
    ImGui::Text("RAM:  %llu MB", systemMemoryMB);
    ImGui::Separator();

    static float ordered[FPS_HISTORY];
    for (int i = 0; i < FPS_HISTORY; ++i)
        ordered[i] = fpsHistory[(fpsIndex + i) % FPS_HISTORY];

    float maxFPS = 60.0f;
    for (float v : ordered) if (v > maxFPS) maxFPS = v;

    ImGui::PlotLines("##fps", ordered, FPS_HISTORY, 0, nullptr,
        0.0f, maxFPS * 1.1f, ImVec2(-1, 80));
    ImGui::End();
}

void ModuleEditor::drawAssetBrowser()
{
    ImGui::Begin("Asset Browser", &showAssetBrowser);

    if (!ImGui::BeginTabBar("##ABTabs")) { ImGui::End(); return; }

    if (ImGui::BeginTabItem("Models"))
    {
        ImGui::SeparatorText("Import");
        static char importBuf[256] = "Assets/Models/";
        ImGui::SetNextItemWidth(-80);
        ImGui::InputText("##imp", importBuf, sizeof(importBuf));
        ImGui::SameLine();
        if (ImGui::Button("Import"))
        {
            app->getAssets()->importAsset(importBuf);
            log(("Imported: " + std::string(importBuf)).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::SeparatorText("Add to Scene");

        static char addBuf[256] = "Assets/Models/Duck/duck.gltf";
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##addmodel", addBuf, sizeof(addBuf));
        if (ImGui::Button("Add to Scene", ImVec2(-1, 0)))
        {
            IScene* active = sceneManager ? sceneManager->getActiveScene() : nullptr;
            ModuleScene* scene = active ? active->getModuleScene() : nullptr;
            if (scene)
            {
                std::string path(addBuf);
                std::string name = std::filesystem::path(path).stem().string();
                GameObject* go = scene->createGameObject(name);
                ComponentMesh* m = go->createComponent<ComponentMesh>();
                if (m->loadModel(addBuf))
                    log(("Added " + name).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
                else
                    log(("Failed: " + std::string(addBuf)).c_str(), ImVec4(1, 0.4f, 0.4f, 1));
            }
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Scenes"))
    {
        std::string scenesPath = app->getFileSystem()->GetLibraryPath() + "Scenes/";
        ImGui::SeparatorText("Saved Scenes");

        try
        {
            namespace fs = std::filesystem;
            bool found = false;
            if (fs::exists(scenesPath))
            {
                for (const auto& e : fs::directory_iterator(scenesPath))
                {
                    if (!e.is_regular_file() || e.path().extension() != ".json") continue;
                    found = true;

                    std::string fname = e.path().filename().string();
                    std::string fpath = e.path().string();
                    ImGui::PushID(fname.c_str());

                    bool sel = false;
                    if (ImGui::Selectable(fname.c_str(), &sel,
                        ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        if (ImGui::IsMouseDoubleClicked(0)
                            && sceneManager && sceneManager->getActiveScene())
                        {
                            if (sceneManager->loadScene(fpath))
                                log(("Loaded: " + fname).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
                        }
                    }
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Load") && sceneManager && sceneManager->getActiveScene())
                            sceneManager->loadScene(fpath);
                        if (ImGui::MenuItem("Delete")) { fs::remove(e.path()); }
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();
                }
            }
            if (!found) ImGui::TextDisabled("No scenes saved yet.");
        }
        catch (const std::exception& ex)
        {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", ex.what());
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        if (ImGui::Button("Quick Save", ImVec2(-1, 0)))
        {
            std::string p = "Library/Scenes/current_scene.json";
            if (sceneManager && sceneManager->saveCurrentScene(p))
                log(("Saved to " + p).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
        }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Prefabs"))
    {
        auto prefabs = PrefabManager::listPrefabs();
        if (prefabs.empty())
        {
            ImGui::TextDisabled("No prefabs yet. Create from the Inspector.");
        }
        else
        {
            for (const auto& name : prefabs)
            {
                ImGui::PushID(name.c_str());
                if (ImGui::Selectable(name.c_str()))
                {
                    IScene* active = sceneManager ? sceneManager->getActiveScene() : nullptr;
                    ModuleScene* scene = active ? active->getModuleScene() : nullptr;
                    if (scene) PrefabManager::instantiatePrefab(name, scene);
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
    ImGui::End();
}

void ModuleEditor::drawSceneSettings()
{
    if (!ImGui::Begin("Scene Settings", &showSceneSettings))
    {
        ImGui::End(); return;
    }

    if (!sceneManager) { ImGui::TextDisabled("No scene manager."); ImGui::End(); return; }

    EditorSceneSettings& s = sceneManager->getSettings();

    if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Show Grid", &s.showGrid);
        ImGui::Checkbox("Show Axis", &s.showAxis);
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Debug Draw Lights", &s.debugDrawLights);
        if (s.debugDrawLights)
            ImGui::SliderFloat("Light Size", &s.debugLightSize, 0.1f, 5.0f);
        ImGui::Separator();

        if (ImGui::TreeNodeEx("Ambient", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::ColorEdit3("Color", &s.ambient.color.x);
            ImGui::SliderFloat("Intensity", &s.ambient.intensity, 0, 2);
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

void ModuleEditor::log(const char* text, const ImVec4& color)
{
    console.push_back({ text, color });
}

void ModuleEditor::updateFPS()
{
    fpsHistory[fpsIndex] = app->getFPS();
    fpsIndex = (fpsIndex + 1) % FPS_HISTORY;
}

void ModuleEditor::updateCameraConstants(const Matrix& view, const Matrix& proj)
{
    CameraConstants c;
    c.viewProj = (view * proj).Transpose();
    void* data = nullptr;
    cameraConstantBuffer->Map(0, nullptr, &data);
    memcpy(data, &c, sizeof(c));
    cameraConstantBuffer->Unmap(0, nullptr);
}

GameObject* ModuleEditor::createEmptyGameObject(const char* name, GameObject* parent)
{
    IScene* active = sceneManager ? sceneManager->getActiveScene() : nullptr;
    ModuleScene* scene = active ? active->getModuleScene() : nullptr;
    if (!scene) return nullptr;

    GameObject* go = scene->createGameObject(name);
    if (parent) go->setParent(parent);

    selectedGameObject = go;
    log(("Created: " + std::string(name)).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
    return go;
}

void ModuleEditor::deleteGameObject(GameObject* go)
{
    if (!go) return;

    if (selectedGameObject == go || isChildOf(go, selectedGameObject))
        selectedGameObject = nullptr;

    IScene* active = sceneManager ? sceneManager->getActiveScene() : nullptr;
    ModuleScene* scene = active ? active->getModuleScene() : nullptr;
    if (!scene) return;

    GameObject* grandparent = go->getParent();
    for (GameObject* child : go->getChildren())
        child->setParent(grandparent);   

    scene->destroyGameObject(go);

    log(("Deleted: " + go->getName()).c_str(), ImVec4(1.0f, 0.7f, 0.4f, 1.0f));
}

void ModuleEditor::updateMemory()
{
    gpuMemoryMB = systemMemoryMB = 0;

    ID3D12Device* device = app->getD3D12()->getDevice();
    if (device)
    {
        ComPtr<IDXGIDevice>  dxgiDev;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIAdapter3> adapter3;
        device->QueryInterface(IID_PPV_ARGS(&dxgiDev));
        dxgiDev->GetAdapter(&adapter);
        adapter.As(&adapter3);
        if (adapter3)
        {
            DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
            adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
            gpuMemoryMB = info.CurrentUsage / (1024 * 1024);
        }
    }

    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    systemMemoryMB = (mem.ullTotalPhys - mem.ullAvailPhys) / (1024 * 1024);
}

bool ModuleEditor::isChildOf(const GameObject* root, const GameObject* needle)
{
    if (!root || !needle) return false;
    if (root == needle)   return true;
    for (const GameObject* child : root->getChildren())
        if (isChildOf(child, needle)) return true;
    return false;
}