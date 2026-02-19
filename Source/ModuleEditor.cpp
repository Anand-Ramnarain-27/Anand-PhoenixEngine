#include "Globals.h"
#include "ModuleEditor.h"
#include "ModuleSamplerHeap.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "Model.h"
#include "Material.h"
#include "ModuleAssets.h"
#include "RenderTexture.h"
#include "DebugDrawPass.h"
#include "ImGuiPass.h"
#include "EmptyScene.h"
#include "ModuleScene.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentMesh.h"
#include "ComponentCamera.h"
#include "ComponentDirectionalLight.h"
#include "ComponentPointLight.h"
#include "ComponentSpotLight.h"
#include "ComponentFactory.h"
#include "PrimitiveFactory.h"
#include "ModuleCamera.h"
#include "PrefabManager.h"
#include "TextureImporter.h"
#include <d3dx12.h>
#include <filesystem>

ModuleEditor::ModuleEditor() = default;
ModuleEditor::~ModuleEditor() = default;

ComPtr<ID3D12Resource> ModuleEditor::createUploadBuffer(ID3D12Device* device, SIZE_T size, const wchar_t* name)
{
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bd = CD3DX12_RESOURCE_DESC::Buffer((size + 255) & ~255);
    ComPtr<ID3D12Resource> buf;
    device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf));
    if (name) buf->SetName(name);
    return buf;
}

bool ModuleEditor::init()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleShaderDescriptors* descs = app->getShaderDescriptors();
    ID3D12Device* device = d3d12->getDevice();

    descTable = descs->allocTable();

    ComPtr<ID3D12Device2> device2;
    ComPtr<ID3D12Device4> device4;
    device->QueryInterface(IID_PPV_ARGS(&device2));
    device->QueryInterface(IID_PPV_ARGS(&device4));

    imguiPass = std::make_unique<ImGuiPass>(device2.Get(), d3d12->getHWnd(), descTable.getCPUHandle(), descTable.getGPUHandle());
    debugDrawPass = std::make_unique<DebugDrawPass>(device4.Get(), d3d12->getDrawCommandQueue(), false);
    viewportRT = std::make_unique<RenderTexture>("EditorViewport", DXGI_FORMAT_R8G8B8A8_UNORM,
        Vector4(0.1f, 0.1f, 0.1f, 1.0f), DXGI_FORMAT_D32_FLOAT, 1.0f);
    sceneManager = std::make_unique<SceneManager>();
    meshPipeline = std::make_unique<MeshPipeline>();

    if (!meshPipeline->init(device)) return false;
    sceneManager->setScene(std::make_unique<EmptyScene>(), device);

    D3D12_QUERY_HEAP_DESC qDesc = { D3D12_QUERY_HEAP_TYPE_TIMESTAMP, 2, 0 };
    device->CreateQueryHeap(&qDesc, IID_PPV_ARGS(&gpuQueryHeap));

    auto rbHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    auto rbDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * 2);
    device->CreateCommittedResource(&rbHeap, D3D12_HEAP_FLAG_NONE, &rbDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&gpuReadbackBuffer));

    cameraConstantBuffer = createUploadBuffer(device, sizeof(CameraConstants), L"CameraCB");
    objectConstantBuffer = createUploadBuffer(device, sizeof(ObjectConstants), L"ObjectCB");
    lightConstantBuffer = createUploadBuffer(device, sizeof(MeshPipeline::LightCB), L"LightCB");

    m_saveDialog.setExtensionFilter(".json");
    m_loadDialog.setExtensionFilter(".json");

    log("[Editor] Initialized", ImVec4(0.6f, 1.0f, 0.6f, 1.0f));
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

ModuleScene* ModuleEditor::getActiveModuleScene() const
{
    IScene* active = sceneManager ? sceneManager->getActiveScene() : nullptr;
    return active ? active->getModuleScene() : nullptr;
}

void ModuleEditor::handleViewportResize()
{
    if (!pendingViewportResize) return;
    if (pendingViewportWidth > 4 && pendingViewportHeight > 4)
    {
        app->getD3D12()->flush();
        viewportRT->resize(pendingViewportWidth, pendingViewportHeight);
        if (sceneManager)
            sceneManager->onViewportResized(pendingViewportWidth, pendingViewportHeight);
    }
    pendingViewportResize = false;
}

void ModuleEditor::handleDialogs()
{
    auto tryScene = [&](bool saved, const char* okMsg, const char* failMsg)
        {
            log(saved ? okMsg : failMsg, saved ? ImVec4(0.6f, 1.0f, 0.6f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        };

    if (m_saveDialog.draw() && sceneManager && sceneManager->getActiveScene())
        tryScene(sceneManager->saveCurrentScene(m_saveDialog.getSelectedPath()), "Scene saved!", "Failed to save scene.");

    if (m_loadDialog.draw() && sceneManager && sceneManager->getActiveScene())
        tryScene(sceneManager->loadScene(m_loadDialog.getSelectedPath()), "Scene loaded!", "Failed to load scene.");
}

void ModuleEditor::preRender()
{
    handleViewportResize();

    imguiPass->startFrame();
    ImGuizmo::BeginFrame();

    if (sceneManager) sceneManager->update(app->getElapsedMilis() * 0.001f);
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

    handleDialogs();

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

void ModuleEditor::handleNewScenePopup(ID3D12GraphicsCommandList* cmd)
{
    if (showNewSceneConfirmation)
    {
        ImGui::OpenPopup("New Scene?");
        showNewSceneConfirmation = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal("New Scene?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

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

void ModuleEditor::render()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleShaderDescriptors* descs = app->getShaderDescriptors();
    ID3D12GraphicsCommandList* cmd = d3d12->getCommandList();

    cmd->Reset(d3d12->getCommandAllocator(), nullptr);
    cmd->EndQuery(gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);

    ID3D12DescriptorHeap* heaps[] = { descs->getHeap(), app->getSamplerHeap()->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    handleNewScenePopup(cmd);

    if (viewportRT && viewportRT->isValid())
        renderViewportToTexture(cmd);

    auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ResourceBarrier(1, &toRT);

    auto rtv = d3d12->getRenderTargetDescriptor();
    cmd->OMSetRenderTargets(1, &rtv, false, nullptr);
    float clear[] = { 0, 0, 0, 1 };
    cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);
    imguiPass->record(cmd);

    auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    cmd->ResourceBarrier(1, &toPresent);

    cmd->EndQuery(gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
    cmd->ResolveQueryData(gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, gpuReadbackBuffer.Get(), 0);
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

void ModuleEditor::gatherLights(GameObject* node, MeshPipeline::LightCB& out) const
{
    if (!node || !node->isActive()) return;

    if (auto* dl = node->getComponent<ComponentDirectionalLight>())
    {
        if (dl->enabled && out.numDirLights < 2)
        {
            auto& gpu = out.dirLights[out.numDirLights++];
            gpu.direction = dl->direction;
            gpu.color = dl->color;
            gpu.intensity = dl->intensity;
        }
    }

    if (auto* pl = node->getComponent<ComponentPointLight>(); pl && pl->enabled && out.numPointLights == 0)
    {
        out.numPointLights = 1;
        out.pointLight.position = node->getTransform()->getGlobalMatrix().Translation();
        out.pointLight.color = pl->color;
        out.pointLight.intensity = pl->intensity;
        out.pointLight.sqRadius = pl->radius * pl->radius;
    }

    if (auto* sl = node->getComponent<ComponentSpotLight>(); sl && sl->enabled && out.numSpotLights == 0)
    {
        out.numSpotLights = 1;
        out.spotLight.position = node->getTransform()->getGlobalMatrix().Translation();
        out.spotLight.direction = sl->direction;
        out.spotLight.color = sl->color;
        out.spotLight.intensity = sl->intensity;
        out.spotLight.sqRadius = sl->radius * sl->radius;
        out.spotLight.innerCos = cosf(sl->innerAngle * 0.0174532925f);
        out.spotLight.outerCos = cosf(sl->outerAngle * 0.0174532925f);
    }

    for (auto* child : node->getChildren())
        gatherLights(child, out);
}

void ModuleEditor::renderViewportToTexture(ID3D12GraphicsCommandList* cmd)
{
    const UINT w = UINT(viewportSize.x), h = UINT(viewportSize.y);
    if (!viewportRT || w == 0 || h == 0) return;

    ModuleCamera* camera = app->getCamera();
    const Matrix& view = camera->getView();
    Matrix        proj = ModuleCamera::getPerspectiveProj(float(w) / float(h));
    const EditorSceneSettings& s = sceneManager->getSettings();
    ModuleScene* moduleScene = getActiveModuleScene();

    viewportRT->beginRender(cmd);
    BEGIN_EVENT(cmd, "Editor Viewport");

    cmd->SetPipelineState(meshPipeline->getPSO());
    cmd->SetGraphicsRootSignature(meshPipeline->getRootSig());
    cmd->SetGraphicsRootDescriptorTable(5, app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::Type(m_samplerType)));

    MeshPipeline::LightCB lightData = {};
    lightData.ambientColor = s.ambient.color;
    lightData.ambientIntensity = s.ambient.intensity;
    lightData.viewPos = camera->getPos();

    if (moduleScene) gatherLights(moduleScene->getRoot(), lightData);

    void* mapped = nullptr;
    lightConstantBuffer->Map(0, nullptr, &mapped);
    memcpy(mapped, &lightData, sizeof(lightData));
    lightConstantBuffer->Unmap(0, nullptr);
    cmd->SetGraphicsRootConstantBufferView(2, lightConstantBuffer->GetGPUVirtualAddress());

    Matrix vp = (view * proj).Transpose();
    Matrix identity = Matrix::Identity.Transpose();
    cmd->SetGraphicsRoot32BitConstants(0, 16, &vp, 0);
    cmd->SetGraphicsRoot32BitConstants(1, 16, &identity, 0);

    if (sceneManager) sceneManager->render(cmd, *camera, w, h);

    if (s.showGrid) dd::xzSquareGrid(-100.0f, 100.0f, 0.0f, 1.0f, dd::colors::Gray);
    if (s.showAxis) { Matrix id = Matrix::Identity; dd::axisTriad(id.m[0], 0.0f, 2.0f, 2.0f); }
    if (s.debugDrawLights && moduleScene) debugDrawLights(moduleScene, s.debugLightSize);

    camera->aspectRatio = (h > 0) ? float(w) / float(h) : 1.0f;

    auto ddVec = [](const Vector3& v) -> const float* { return &v.x; };

    if (moduleScene && s.debugDrawCameraFrustums)
    {
        std::function<void(GameObject*)> visitCams = [&](GameObject* node)
            {
                if (!node || !node->isActive()) return;
                if (auto* cam = node->getComponent<ComponentCamera>())
                {
                    if (auto* t = node->getTransform())
                    {
                        Matrix world = t->getGlobalMatrix();
                        Vector3 pos = world.Translation();
                        Vector3 fwd = -Vector3(world.m[2][0], world.m[2][1], world.m[2][2]); fwd.Normalize();
                        Vector3 right = Vector3(world.m[0][0], world.m[0][1], world.m[0][2]); right.Normalize();
                        Vector3 up = Vector3(world.m[1][0], world.m[1][1], world.m[1][2]); up.Normalize();

                        float aspect = (h > 0) ? float(w) / float(h) : 1.0f;
                        Frustum f = Frustum::fromCamera(pos, fwd, right, up,
                            cam->getFOV(), aspect, cam->getNearPlane(), cam->getFarPlane());

                        if (cam->isMainCamera())
                            camera->setGameCameraFrustum(f);

                        ddVec(pos); 
                        const auto& c = f.corners;
                        using CI = Frustum::CornerIdx;
                        auto col = cam->isMainCamera() ? dd::colors::Yellow : dd::colors::Cyan;

                        dd::line(ddVec(c[CI::NTL]), ddVec(c[CI::NTR]), col);
                        dd::line(ddVec(c[CI::NTR]), ddVec(c[CI::NBR]), col);
                        dd::line(ddVec(c[CI::NBR]), ddVec(c[CI::NBL]), col);
                        dd::line(ddVec(c[CI::NBL]), ddVec(c[CI::NTL]), col);

                        dd::line(ddVec(c[CI::FTL]), ddVec(c[CI::FTR]), col);
                        dd::line(ddVec(c[CI::FTR]), ddVec(c[CI::FBR]), col);
                        dd::line(ddVec(c[CI::FBR]), ddVec(c[CI::FBL]), col);
                        dd::line(ddVec(c[CI::FBL]), ddVec(c[CI::FTL]), col);

                        dd::line(ddVec(c[CI::NTL]), ddVec(c[CI::FTL]), col);
                        dd::line(ddVec(c[CI::NTR]), ddVec(c[CI::FTR]), col);
                        dd::line(ddVec(c[CI::NBL]), ddVec(c[CI::FBL]), col);
                        dd::line(ddVec(c[CI::NBR]), ddVec(c[CI::FBR]), col);

                        dd::line(ddVec(pos), ddVec(pos + right * 0.4f), dd::colors::Red);
                        dd::line(ddVec(pos), ddVec(pos + up * 0.4f), dd::colors::Green);
                        dd::line(ddVec(pos), ddVec(pos + fwd * 0.4f), dd::colors::Blue);

                        dd::line(ddVec(pos), ddVec(pos + fwd * cam->getNearPlane()), dd::colors::LightGoldenYellow);
                        Vector3 farEnd = pos + fwd * std::min(cam->getFarPlane(), 50.0f);
                        dd::line(ddVec(pos + fwd * cam->getNearPlane()), ddVec(farEnd), dd::colors::DarkCyan);
                    }
                }
                for (auto* child : node->getChildren()) visitCams(child);
            };
        visitCams(moduleScene->getRoot());
    }

    if (s.debugDrawEditorCameraRay)
    {
        Vector3 edPos = camera->getPos();
        Vector3 edFwd = camera->getForward();
        Vector3 nearPt = edPos + edFwd * camera->nearZ;
        Vector3 farPt = edPos + edFwd * std::min(camera->farZ, 20.0f);
        dd::line(ddVec(edPos), ddVec(nearPt), dd::colors::White);
        dd::line(ddVec(nearPt), ddVec(farPt), dd::colors::DarkGray);
    }

    debugDrawPass->record(cmd, w, h, view, proj);
    END_EVENT(cmd);
    viewportRT->endRender(cmd);
}

void ModuleEditor::drawDockspace()
{
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##MainDockspace", nullptr, kFlags);
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
        if (ImGui::MenuItem("New Scene", "Ctrl+N"))         showNewSceneConfirmation = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Save Scene", "Ctrl+S"))         m_saveDialog.open(FileDialog::Type::Save, "Save Scene", "Library/Scenes");
        if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))  m_saveDialog.open(FileDialog::Type::Save, "Save Scene", "Library/Scenes/");
        ImGui::Separator();
        if (ImGui::MenuItem("Load Scene...", "Ctrl+O"))         m_loadDialog.open(FileDialog::Type::Open, "Load Scene", "Library/Scenes/");
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")) {}
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Create Empty GameObject", "Ctrl+Shift+N")) createEmptyGameObject();
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
        if (ImGui::MenuItem("Create Empty"))                      createEmptyGameObject();
        if (ImGui::MenuItem("Create Empty Child") && selectedGameObject) createEmptyGameObject("Empty", selectedGameObject);
        ImGui::Separator();

        auto tryAddComponent = [&](const char* label, Component::Type type, auto* existing)
            {
                if (ImGui::MenuItem(label) && selectedGameObject && !existing)
                    selectedGameObject->addComponent(ComponentFactory::CreateComponent(type, selectedGameObject));
            };
        tryAddComponent("Add Camera Component", Component::Type::Camera, selectedGameObject ? selectedGameObject->getComponent<ComponentCamera>() : nullptr);
        tryAddComponent("Add Mesh Component", Component::Type::Mesh, selectedGameObject ? selectedGameObject->getComponent<ComponentMesh>() : nullptr);

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene"))
    {
        if (ImGui::MenuItem("Play", nullptr, false, sceneManager && !sceneManager->isPlaying())) sceneManager->play();
        if (ImGui::MenuItem("Pause", nullptr, false, sceneManager && sceneManager->isPlaying())) sceneManager->pause();
        if (ImGui::MenuItem("Stop", nullptr, false, sceneManager && sceneManager->getState() != SceneManager::PlayState::Stopped)) sceneManager->stop();
        ImGui::EndMenu();
    }

    {
        const float btnW = 70.0f;
        const float totalW = btnW * 3 + ImGui::GetStyle().ItemSpacing.x * 2;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - totalW) * 0.5f);

        bool playing = sceneManager && sceneManager->isPlaying();
        bool paused = sceneManager && sceneManager->getState() == SceneManager::PlayState::Paused;

        if (playing) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1));
        if (ImGui::Button("  Play  ", ImVec2(btnW, 0)) && sceneManager && !playing) sceneManager->play();
        if (playing) ImGui::PopStyleColor();

        ImGui::SameLine();
        if (paused) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.7f, 0.1f, 1));
        if (ImGui::Button(" Pause  ", ImVec2(btnW, 0)) && sceneManager && playing) sceneManager->pause();
        if (paused) ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button("  Stop  ", ImVec2(btnW, 0)) && sceneManager) sceneManager->stop();
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
        if (ImGui::IsKeyPressed(ImGuiKey_G)) gizmoMode = (gizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
    }

    ImGuiWindow* win = ImGui::FindWindowByName("Viewport");
    if (!win) return;

    ImGui::SetNextWindowPos({ win->Pos.x + 8, win->Pos.y + 28 }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.7f);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing;

    if (!ImGui::Begin("##GizmoToolbar", nullptr, kFlags)) { ImGui::End(); return; }

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
        if (gizmoOperation == ImGuizmo::TRANSLATE) ImGui::TextDisabled("%.2f", snapTranslate[0]);
        else if (gizmoOperation == ImGuizmo::ROTATE)    ImGui::TextDisabled("%.0f°", snapRotate);
        else                                            ImGui::TextDisabled("%.2f", snapScale);
    }

    ImGui::End();
}

void ModuleEditor::drawHierarchy()
{
    ImGui::Begin("Hierarchy", &showHierarchy);

    ModuleScene* scene = getActiveModuleScene();
    if (!scene) { ImGui::End(); return; }

    if (ImGui::Button("+ Empty"))                          createEmptyGameObject();
    ImGui::SameLine();
    if (ImGui::Button("+ Child") && selectedGameObject)    createEmptyGameObject("Empty", selectedGameObject);
    ImGui::Separator();

    drawHierarchyNode(scene->getRoot());

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered())
        ImGui::OpenPopup("##HierarchyBlank");

    hierarchyBlankContextMenu();
    ImGui::End();
}

void ModuleEditor::drawHierarchyNode(GameObject* go)
{
    if (!go) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (go == selectedGameObject) flags |= ImGuiTreeNodeFlags_Selected;
    if (go->getChildren().empty()) flags |= ImGuiTreeNodeFlags_Leaf;

    if (renamingObject && renamingTarget == go)
    {
        ImGui::SetKeyboardFocusHere();
        bool confirmed = ImGui::InputText("##rename", renameBuffer, sizeof(renameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (confirmed || ImGui::IsItemDeactivated())
        {
            go->setName(renameBuffer);
            renamingObject = false;
            renamingTarget = nullptr;
        }
        for (auto* child : go->getChildren()) drawHierarchyNode(child);
        return;
    }

    bool open = ImGui::TreeNodeEx((void*)(uintptr_t)go->getUID(), flags, go->getName().c_str());

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())    selectedGameObject = go;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))              selectedGameObject = go;

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
    {
        renamingObject = true;
        renamingTarget = go;
        strncpy_s(renameBuffer, go->getName().c_str(), sizeof(renameBuffer) - 1);
    }

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
            if (dragged && dragged != go) dragged->setParent(go);
        }
        ImGui::EndDragDropTarget();
    }

    if (open)
    {
        for (auto* child : go->getChildren()) drawHierarchyNode(child);
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

    if (ImGui::BeginMenu("Add Component"))
    {
        auto addIf = [&](const char* label, Component::Type type, bool hasIt)
            {
                if (ImGui::MenuItem(label) && !hasIt)
                {
                    go->addComponent(ComponentFactory::CreateComponent(type, go));
                    ImGui::CloseCurrentPopup();
                }
            };
        addIf("Camera", Component::Type::Camera, go->getComponent<ComponentCamera>() != nullptr);
        addIf("Mesh", Component::Type::Mesh, go->getComponent<ComponentMesh>() != nullptr);
        ImGui::Separator();
        addIf("Directional Light", Component::Type::DirectionalLight, go->getComponent<ComponentDirectionalLight>() != nullptr);
        addIf("Point Light", Component::Type::PointLight, go->getComponent<ComponentPointLight>() != nullptr);
        addIf("Spot Light", Component::Type::SpotLight, go->getComponent<ComponentSpotLight>() != nullptr);
        ImGui::EndMenu();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Create Empty Child")) createEmptyGameObject("Empty", go);
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    if (ImGui::MenuItem("Delete")) deleteGameObject(go);
    ImGui::PopStyleColor();
}

void ModuleEditor::hierarchyBlankContextMenu()
{
    if (!ImGui::BeginPopup("##HierarchyBlank")) return;
    if (ImGui::MenuItem("Create Empty GameObject"))             createEmptyGameObject();
    if (ImGui::MenuItem("Create Empty Child") && selectedGameObject) createEmptyGameObject("Empty", selectedGameObject);
    ImGui::EndPopup();
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
    if (ImGui::Checkbox("##active", &active)) selectedGameObject->setActive(active);
    ImGui::SameLine();

    char nameBuf[256];
    strncpy_s(nameBuf, selectedGameObject->getName().c_str(), sizeof(nameBuf) - 1);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##goname", nameBuf, sizeof(nameBuf)))
        selectedGameObject->setName(nameBuf);

    ImGui::TextDisabled("UID: %u", selectedGameObject->getUID());
    ImGui::Separator();

    if (ComponentTransform* t = selectedGameObject->getTransform(); t && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float pos[3] = { t->position.x, t->position.y, t->position.z };
        if (ImGui::DragFloat3("Position", pos, 0.1f))
        {
            t->position = { pos[0], pos[1], pos[2] }; t->markDirty();
        }

        Vector3 euler = t->rotation.ToEuler();
        float deg[3] = { euler.x * 57.2957795f, euler.y * 57.2957795f, euler.z * 57.2957795f };
        if (ImGui::DragFloat3("Rotation", deg, 0.5f))
        {
            t->rotation = Quaternion::CreateFromYawPitchRoll(deg[1] * 0.0174532925f, deg[0] * 0.0174532925f, deg[2] * 0.0174532925f);
            t->markDirty();
        }

        float scl[3] = { t->scale.x, t->scale.y, t->scale.z };
        if (ImGui::DragFloat3("Scale", scl, 0.01f))
        {
            t->scale = { scl[0], scl[1], scl[2] }; t->markDirty();
        }
    }

    Component::Type toRemove = Component::Type::Transform;
    bool            wantsRemove = false;

    for (const auto& comp : selectedGameObject->getComponents())
    {
        if (comp->getType() == Component::Type::Transform) continue;

        ImGui::PushID((int)comp->getType());

        const char* label =
            comp->getType() == Component::Type::Camera ? "Camera" :
            comp->getType() == Component::Type::Mesh ? "Mesh" :
            comp->getType() == Component::Type::DirectionalLight ? "Directional Light" :
            comp->getType() == Component::Type::PointLight ? "Point Light" :
            comp->getType() == Component::Type::SpotLight ? "Spot Light" : "Component";

        bool headerOpen = true;
        bool open = ImGui::CollapsingHeader(label, &headerOpen,
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_ClipLabelForTrailingButton);

        if (!headerOpen) { toRemove = comp->getType(); wantsRemove = true; }

        if (open)
        {
            if (comp->getType() == Component::Type::Camera) drawComponentCamera(static_cast<ComponentCamera*>(comp.get()));
            else if (comp->getType() == Component::Type::Mesh)   drawComponentMesh(static_cast<ComponentMesh*>(comp.get()));
            else                                                  comp->onEditor();
        }

        if (ImGui::BeginPopupContextItem("##compctx"))
        {
            ImGui::TextDisabled("%s", label);
            ImGui::Separator();
            if (ImGui::MenuItem("Remove Component")) { toRemove = comp->getType(); wantsRemove = true; }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    if (wantsRemove && toRemove != Component::Type::Transform)
        selectedGameObject->removeComponentByType(toRemove);

    ImGui::Spacing(); ImGui::Separator();
    const float btnW = 180.0f;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - btnW) * 0.5f + ImGui::GetCursorPosX());
    if (ImGui::Button("Add Component", ImVec2(btnW, 0))) ImGui::OpenPopup("##AddComp");

    if (ImGui::BeginPopup("##AddComp"))
    {
        auto addComp = [&](const char* label, Component::Type type, bool hasIt)
            {
                if (ImGui::MenuItem(label, nullptr, false, !hasIt))
                {
                    selectedGameObject->addComponent(ComponentFactory::CreateComponent(type, selectedGameObject));
                    ImGui::CloseCurrentPopup();
                }
            };
        addComp("Camera", Component::Type::Camera, selectedGameObject->getComponent<ComponentCamera>() != nullptr);
        addComp("Mesh", Component::Type::Mesh, selectedGameObject->getComponent<ComponentMesh>() != nullptr);
        ImGui::Separator();
        addComp("Directional Light", Component::Type::DirectionalLight, selectedGameObject->getComponent<ComponentDirectionalLight>() != nullptr);
        addComp("Point Light", Component::Type::PointLight, selectedGameObject->getComponent<ComponentPointLight>() != nullptr);
        addComp("Spot Light", Component::Type::SpotLight, selectedGameObject->getComponent<ComponentSpotLight>() != nullptr);
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
    if (ImGui::Checkbox("Main Camera", &isMain)) cam->setMainCamera(isMain);
    if (isMain)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0.85f, 0.1f, 1), "(culling source)");
    }

    float fovDeg = cam->getFOV() * 57.2957795f;
    if (ImGui::SliderFloat("FOV", &fovDeg, 10.0f, 170.0f)) cam->setFOV(fovDeg * 0.0174532925f);

    float nearP = cam->getNearPlane(), farP = cam->getFarPlane();
    if (ImGui::DragFloat("Near Plane", &nearP, 0.01f, 0.001f, farP - 0.01f))  cam->setNearPlane(nearP);
    if (ImGui::DragFloat("Far Plane", &farP, 1.0f, nearP + 0.01f, 10000.f)) cam->setFarPlane(farP);

    Vector4 bg = cam->getBackgroundColor();
    if (ImGui::ColorEdit4("Background", &bg.x)) cam->setBackgroundColor(bg);

    ImGui::Separator();

    ModuleCamera* edCam = app->getCamera();
    if (edCam)
    {
        ImGui::Text("Frustum Culling");
        int cm = (int)edCam->cullMode;
        ImGui::SameLine();
        if (ImGui::RadioButton("Off##fc", &cm, 0)) edCam->cullMode = ModuleCamera::CullMode::None;
        ImGui::SameLine();
        if (ImGui::RadioButton("Frustum##fc", &cm, 1)) edCam->cullMode = ModuleCamera::CullMode::Frustum;

        ImGui::Text("Cull from");
        int cs = (int)edCam->cullSource;
        ImGui::SameLine();
        if (ImGui::RadioButton("Editor##cs", &cs, 0)) edCam->cullSource = ModuleCamera::CullSource::EditorCamera;
        ImGui::SameLine();
        if (ImGui::RadioButton("This Cam##cs", &cs, 1))
        {
            edCam->cullSource = ModuleCamera::CullSource::GameCamera;
            cam->setMainCamera(true); 
        }

        if (edCam->cullSource == ModuleCamera::CullSource::GameCamera && !cam->isMainCamera())
            ImGui::TextColored(ImVec4(1, 0.4f, 0.1f, 1), "  Check 'Main Camera' to use for culling");
    }

    ImGui::Separator();
    if (selectedGameObject)
    {
        if (auto* t = selectedGameObject->getTransform())
        {
            Vector3 p = t->getGlobalMatrix().Translation();
            ImGui::TextDisabled("Pos: %.2f  %.2f  %.2f", p.x, p.y, p.z);
        }
    }
}

void ModuleEditor::drawComponentMesh(ComponentMesh* mesh)
{
    if (mesh->getModel()) ImGui::TextDisabled("Model loaded");
    else                  ImGui::TextColored(ImVec4(1, 0.6f, 0.4f, 1), "No model loaded");

    static char meshPathBuf[256] = "";
    ImGui::SetNextItemWidth(-80);
    ImGui::InputText("##meshpath", meshPathBuf, sizeof(meshPathBuf));
    ImGui::SameLine();
    if (ImGui::Button("Load") && strlen(meshPathBuf) > 0)
    {
        bool ok = mesh->loadModel(meshPathBuf);
        log(ok ? ("Loaded: " + std::string(meshPathBuf)).c_str()
            : ("Failed to load: " + std::string(meshPathBuf)).c_str(),
            ok ? ImVec4(0.6f, 1, 0.6f, 1) : ImVec4(1, 0.4f, 0.4f, 1));
    }
}

void ModuleEditor::drawGizmo()
{
    if (!selectedGameObject) return;
    ComponentTransform* t = selectedGameObject->getTransform();
    if (!t) return;
    ModuleCamera* camera = app->getCamera();
    if (!camera) return;

    const float w = viewportSize.x, h = viewportSize.y;
    if (w <= 0.0f || h <= 0.0f) return;

    Matrix view = camera->getView();
    Matrix proj = ModuleCamera::getPerspectiveProj(w / h);
    Matrix world = t->getGlobalMatrix();

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewportPos.x, viewportPos.y, w, h);

    float snapVals[3] = {};
    float* snapPtr = nullptr;
    if (useSnap)
    {
        if (gizmoOperation == ImGuizmo::TRANSLATE) { snapVals[0] = snapTranslate[0]; snapVals[1] = snapTranslate[1]; snapVals[2] = snapTranslate[2]; }
        else if (gizmoOperation == ImGuizmo::ROTATE) { snapVals[0] = snapRotate; }
        else { snapVals[0] = snapScale; }
        snapPtr = snapVals;
    }

    bool manipulated = ImGuizmo::Manipulate(
        reinterpret_cast<const float*>(&view), reinterpret_cast<const float*>(&proj),
        gizmoOperation, gizmoMode, reinterpret_cast<float*>(&world), nullptr, snapPtr);

    if (manipulated)
    {
        Matrix localWorld = world;
        if (GameObject* par = selectedGameObject->getParent())
            localWorld = world * par->getTransform()->getGlobalMatrix().Invert();

        float translation[3], eulerDeg[3], scale[3];
        ImGuizmo::DecomposeMatrixToComponents(reinterpret_cast<const float*>(&localWorld), translation, eulerDeg, scale);

        t->position = { translation[0], translation[1], translation[2] };
        t->scale = { scale[0], scale[1], scale[2] };
        t->rotation = Quaternion::CreateFromYawPitchRoll(eulerDeg[1] * 0.0174532925f, eulerDeg[0] * 0.0174532925f, eulerDeg[2] * 0.0174532925f);
        t->markDirty();
    }
}

void ModuleEditor::drawViewport()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("Viewport", &showViewport, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

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

    char buf[160];
    sprintf_s(buf, "FPS: %.1f  CPU: %.2f ms  GPU: %.2f ms", app->getFPS(), app->getAvgElapsedMs(), gpuFrameTimeMs);
    ImGui::GetForegroundDrawList()->AddText({ win->Pos.x + 10, win->Pos.y + win->Size.y - 24 }, IM_COL32(0, 230, 0, 220), buf);
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
    if (gpuTimerReady) ImGui::Text("GPU:  %.2f ms", gpuFrameTimeMs);
    ImGui::Separator();
    ImGui::Text("VRAM: %llu MB", gpuMemoryMB);
    ImGui::Text("RAM:  %llu MB", systemMemoryMB);
    ImGui::Separator();

    float ordered[FPS_HISTORY];
    float maxFPS = 60.0f;
    for (int i = 0; i < FPS_HISTORY; ++i)
    {
        ordered[i] = fpsHistory[(fpsIndex + i) % FPS_HISTORY];
        if (ordered[i] > maxFPS) maxFPS = ordered[i];
    }
    ImGui::PlotLines("##fps", ordered, FPS_HISTORY, 0, nullptr, 0.0f, maxFPS * 1.1f, ImVec2(-1, 80));
    ImGui::End();
}

void ModuleEditor::drawAssetBrowser()
{
    ImGui::Begin("Asset Browser", &showAssetBrowser);

    static int abFilter = 0;
    const char* filters[] = { "All", "Models", "Textures", "Scenes", "Prefabs" };

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    ImGui::BeginChild("##ABToolbar", ImVec2(0, 32), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos({ 6, 6 });
    for (int i = 0; i < 5; i++)
    {
        bool active = (abFilter == i);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(0.26f, 0.59f, 0.98f, 1.0f) : ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
        if (ImGui::Button(filters[i], ImVec2(60, 20))) abFilter = i;
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 2);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Separator();

    const float panelW = 180.0f;
    const float actionBarH = 20.0f;
    const float panelH = ImGui::GetContentRegionAvail().y - actionBarH - 8.0f;
    const float contentW = ImGui::GetContentRegionAvail().x - panelW - 6.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
    ImGui::BeginChild("##ABLeft", ImVec2(panelW, panelH));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::SetCursorPosX(8); ImGui::Text("IMPORT");
    ImGui::PopStyleColor();
    ImGui::Separator(); ImGui::Spacing();

    static char importModelBuf[256] = "Assets/Models/";
    ImGui::SetNextItemWidth(panelW - 16);
    ImGui::SetCursorPosX(8); ImGui::InputText("##impModel", importModelBuf, sizeof(importModelBuf));
    ImGui::SetCursorPosX(8);
    const float halfBtn = (panelW - 20) * 0.5f;
    if (ImGui::Button("Browse##model", ImVec2(halfBtn, 0))) m_modelBrowseDialog.open(FileDialog::Type::Open, "Select Model", "Assets/Models");
    ImGui::SameLine(0, 4);
    if (ImGui::Button("Import##model", ImVec2(halfBtn, 0)) && strlen(importModelBuf) > 0)
    {
        app->getAssets()->importAsset(importModelBuf);
        log(("Imported model: " + std::string(importModelBuf)).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
    }
    if (m_modelBrowseDialog.draw())
        strncpy_s(importModelBuf, m_modelBrowseDialog.getSelectedPath().c_str(), sizeof(importModelBuf) - 1);

    if (abFilter == 0 || abFilter == 2)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::SetCursorPosX(8); ImGui::Text("Texture (.png/.dds)");
        ImGui::PopStyleColor();

        static char importTexBuf[256] = "Assets/Textures/";
        ImGui::SetNextItemWidth(panelW - 16);
        ImGui::SetCursorPosX(8); ImGui::InputText("##impTex", importTexBuf, sizeof(importTexBuf));
        ImGui::SetCursorPosX(8);
        if (ImGui::Button("Browse##tex", ImVec2(halfBtn, 0))) m_texBrowseDialog.open(FileDialog::Type::Open, "Select Texture", "Assets/Textures");
        ImGui::SameLine(0, 4);
        if (ImGui::Button("Import##tex", ImVec2(halfBtn, 0)) && strlen(importTexBuf) > 0)
        {
            std::string name = TextureImporter::GetTextureName(importTexBuf);
            std::string outDir = app->getFileSystem()->GetLibraryPath() + "Textures/";
            app->getFileSystem()->CreateDir(outDir.c_str());
            bool ok = TextureImporter::Import(importTexBuf, outDir + name + ".dds");
            log(ok ? ("Texture imported: " + name).c_str()
                : ("Failed: " + std::string(importTexBuf)).c_str(),
                ok ? ImVec4(0.6f, 1, 0.6f, 1) : ImVec4(1, 0.4f, 0.4f, 1));
        }
        if (m_texBrowseDialog.draw())
            strncpy_s(importTexBuf, m_texBrowseDialog.getSelectedPath().c_str(), sizeof(importTexBuf) - 1);
        ImGui::Spacing();
    }

    if (abFilter == 0 || abFilter == 3)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::SetCursorPosX(8); ImGui::Text("Scene");
        ImGui::PopStyleColor();
        ImGui::SetCursorPosX(8);
        if (ImGui::Button("Quick Save Scene", ImVec2(panelW - 16, 0)))
        {
            std::string p = "Library/Scenes/current_scene.json";
            if (sceneManager && sceneManager->saveCurrentScene(p))
                log(("Saved: " + p).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
        }
        ImGui::SetCursorPosX(8);
        if (ImGui::Button("Save As...", ImVec2(panelW - 16, 0))) m_saveDialog.open(FileDialog::Type::Save, "Save Scene", "Library/Scenes");
        ImGui::SetCursorPosX(8);
        if (ImGui::Button("Load Scene...", ImVec2(panelW - 16, 0))) m_loadDialog.open(FileDialog::Type::Open, "Load Scene", "Library/Scenes");
        ImGui::Spacing();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.11f, 0.11f, 1.0f));
    ImGui::BeginChild("##ABRight", ImVec2(contentW, panelH));
    ImGui::Spacing();

    static char searchBuf[128] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##absearch", "Search...", searchBuf, sizeof(searchBuf));
    ImGui::Separator();

    std::string search(searchBuf);
    std::transform(search.begin(), search.end(), search.begin(), ::tolower);

    static int         selectedAsset = -1;
    static std::string selectedAssetPath = "";
    static std::string selectedAssetType = "";
    int assetIdx = 0;

    auto drawAssetRow = [&](const std::string& path, const std::string& icon, const std::string& type, const std::string& extra = "") -> bool
        {
            std::string fname = std::filesystem::path(path).filename().string();
            std::string lower = fname;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (!search.empty() && lower.find(search) == std::string::npos) { ++assetIdx; return false; }

            bool selected = (selectedAsset == assetIdx);
            ImGui::PushID(assetIdx);
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.26f, 0.59f, 0.98f, 0.4f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.25f));
            bool clicked = ImGui::Selectable("##row", selected,
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, 20));
            ImGui::PopStyleColor(2);

            if (clicked) { selectedAsset = assetIdx; selectedAssetPath = path; selectedAssetType = type; }

            ImGui::SameLine(8);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
            ImGui::Text("%s", icon.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(36);
            ImGui::Text("%s", fname.c_str());
            if (!extra.empty())
            {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1.0f));
                ImGui::Text("  %s", extra.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::PopID();
            ++assetIdx;
            return clicked;
        };

    auto sectionHeader = [](const char* label)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::Text("  %s", label);
            ImGui::PopStyleColor();
            ImGui::Separator();
        };

    auto emptyMsg = [](const char* msg)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
            ImGui::Text("    %s", msg);
            ImGui::PopStyleColor();
        };

    if (abFilter == 0 || abFilter == 1)
    {
        sectionHeader("MODELS");
        std::string modPath = app->getFileSystem()->GetLibraryPath() + "Meshes/";
        bool any = false;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(modPath))
            {
                if (!entry.is_directory()) continue;
                drawAssetRow(entry.path().string(), "[M]", "model", entry.path().filename().string());
                any = true;
            }
        }
        catch (...) {}
        if (!any) emptyMsg("No models imported yet.");
        ImGui::Spacing();
    }

    if (abFilter == 0 || abFilter == 2)
    {
        sectionHeader("TEXTURES");
        auto texFiles = app->getFileSystem()->GetFilesInDirectory(
            (app->getFileSystem()->GetLibraryPath() + "Textures/").c_str(), ".dds");
        if (texFiles.empty()) emptyMsg("No textures imported yet.");
        else for (const auto& f : texFiles) drawAssetRow(f, "[T]", "texture");
        ImGui::Spacing();
    }

    if (abFilter == 0 || abFilter == 3)
    {
        sectionHeader("SCENES");
        auto sceneFiles = app->getFileSystem()->GetFilesInDirectory(
            (app->getFileSystem()->GetLibraryPath() + "Scenes/").c_str(), ".json");
        if (sceneFiles.empty()) emptyMsg("No scenes saved yet.");
        else
        {
            for (const auto& f : sceneFiles)
            {
                bool dbl = drawAssetRow(f, "[S]", "scene");
                if (dbl && ImGui::IsMouseDoubleClicked(0) && sceneManager)
                    if (sceneManager->loadScene(f)) log(("Loaded scene: " + f).c_str(), ImVec4(0.6f, 1, 0.6f, 1));

                if (ImGui::BeginPopupContextItem(("##sc" + f).c_str()))
                {
                    if (ImGui::MenuItem("Load") && sceneManager) sceneManager->loadScene(f);
                    if (ImGui::MenuItem("Delete")) app->getFileSystem()->Delete(f.c_str());
                    ImGui::EndPopup();
                }
            }
        }
        ImGui::Spacing();
    }

    if (abFilter == 0 || abFilter == 4)
    {
        sectionHeader("PREFABS");
        auto prefabs = PrefabManager::listPrefabs();
        if (prefabs.empty()) emptyMsg("No prefabs yet.");
        else for (const auto& name : prefabs) drawAssetRow(name, "[P]", "prefab");
        ImGui::Spacing();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Separator();

    bool hasSelection = !selectedAssetPath.empty();

    if (hasSelection)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
        ImGui::Text("Selected: %s", std::filesystem::path(selectedAssetPath).filename().string().c_str());
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        ImGui::Text("No asset selected");
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();

    if (!hasSelection) ImGui::BeginDisabled();

    const float btnX = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + btnX - 220);

    if (ImGui::Button("Add to Scene", ImVec2(110, 0)) && hasSelection)
    {
        ModuleScene* scene = getActiveModuleScene();
        if (scene)
        {
            if (selectedAssetType == "model")
            {
                std::string name = std::filesystem::path(selectedAssetPath).filename().string();
                std::string gltf = "Assets/Models/" + name + "/" + name + ".gltf";
                GameObject* go = scene->createGameObject(name);
                ComponentMesh* m = go->createComponent<ComponentMesh>();
                bool ok = m->loadModel(gltf.c_str());
                log(ok ? ("Added model: " + name).c_str()
                    : ("Failed to load model: " + gltf).c_str(),
                    ok ? ImVec4(0.6f, 1, 0.6f, 1) : ImVec4(1, 0.4f, 0.4f, 1));
                selectedGameObject = go;
            }
            else if (selectedAssetType == "texture")
            {
                if (m_pendingTexturePath != selectedAssetPath)
                {
                    m_pendingTexture.Reset();
                    if (!TextureImporter::Load(selectedAssetPath, m_pendingTexture, m_pendingTextureSRV))
                    {
                        log("Failed to load texture.", ImVec4(1, 0.4f, 0.4f, 1));
                        m_pendingTexturePath.clear();
                    }
                    else m_pendingTexturePath = selectedAssetPath;
                }

                if (m_pendingTexture)
                {
                    std::string stem = std::filesystem::path(selectedAssetPath).stem().string();
                    if (selectedGameObject)
                    {
                        ComponentMesh* existing = selectedGameObject->getComponent<ComponentMesh>();
                        if (existing && existing->getModel())
                        {
                            for (auto& mat : existing->getModel()->getMaterials())
                                mat->setBaseColorTexture(m_pendingTexture, m_pendingTextureSRV);
                            existing->rebuildMaterialBuffers();
                            log(("Applied texture to: " + selectedGameObject->getName()).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
                        }
                        else
                        {
                            if (!existing) existing = selectedGameObject->createComponent<ComponentMesh>();
                            existing->setModel(PrimitiveFactory::createTexturedQuad(m_pendingTexture, m_pendingTextureSRV));
                            existing->rebuildMaterialBuffers();
                            log(("Created quad on: " + selectedGameObject->getName()).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
                        }
                    }
                    else
                    {
                        selectedGameObject = PrimitiveFactory::createTexturedQuadObject(scene, stem, m_pendingTexture, m_pendingTextureSRV);
                        log(("Added image to scene: " + stem).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
                    }
                }
            }
            else if (selectedAssetType == "scene")
            {
                if (sceneManager->loadScene(selectedAssetPath))
                    log(("Loaded scene: " + selectedAssetPath).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
            }
            else if (selectedAssetType == "prefab")
            {
                PrefabManager::instantiatePrefab(std::filesystem::path(selectedAssetPath).stem().string(), scene);
                log(("Instantiated prefab: " + selectedAssetPath).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
            }
        }
    }

    ImGui::SameLine(0, 4);
    if (ImGui::Button("Delete", ImVec2(100, 0)) && hasSelection)
    {
        if (selectedAssetType == "scene" || selectedAssetType == "texture")
        {
            app->getFileSystem()->Delete(selectedAssetPath.c_str());
            log(("Deleted: " + selectedAssetPath).c_str(), ImVec4(1, 0.6f, 0.4f, 1));
            selectedAsset = -1; selectedAssetPath = ""; selectedAssetType = "";
        }
    }

    if (!hasSelection) ImGui::EndDisabled();
    ImGui::End();
}

void ModuleEditor::drawSceneSettings()
{
    if (!ImGui::Begin("Scene Settings", &showSceneSettings)) { ImGui::End(); return; }
    if (!sceneManager) { ImGui::TextDisabled("No scene manager."); ImGui::End(); return; }

    EditorSceneSettings& s = sceneManager->getSettings();

    if (ImGui::CollapsingHeader("Camera Debug", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Draw Camera Frustums", &s.debugDrawCameraFrustums);
        ImGui::Checkbox("Draw Editor Camera Ray", &s.debugDrawEditorCameraRay);

        ImGui::Separator();
        if (ModuleCamera* cam = app->getCamera())
            cam->onEditorDebugPanel();
    }

    if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Show Grid", &s.showGrid);
        ImGui::Checkbox("Show Axis", &s.showAxis);
        ImGui::Separator();
        ImGui::Text("Texture Sampler");
        ImGui::Combo("##sampler", &m_samplerType, "Linear/Wrap\0Point/Wrap\0Linear/Clamp\0Point/Clamp\0");
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Debug Draw Lights", &s.debugDrawLights);
        if (s.debugDrawLights) ImGui::SliderFloat("Light Size", &s.debugLightSize, 0.1f, 5.0f);
        ImGui::Separator();

        if (ImGui::TreeNodeEx("Ambient", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::ColorEdit3("Color", &s.ambient.color.x);
            ImGui::SliderFloat("Intensity", &s.ambient.intensity, 0, 2);
            ImGui::TreePop();
        }
        ImGui::TextDisabled("Add light components to GameObjects via Inspector.");
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
    CameraConstants c = { (view * proj).Transpose() };
    void* data = nullptr;
    cameraConstantBuffer->Map(0, nullptr, &data);
    memcpy(data, &c, sizeof(c));
    cameraConstantBuffer->Unmap(0, nullptr);
}

void ModuleEditor::updateMemory()
{
    gpuMemoryMB = systemMemoryMB = 0;

    if (ID3D12Device* device = app->getD3D12()->getDevice())
    {
        ComPtr<IDXGIDevice> dxgiDev;
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

    MEMORYSTATUSEX mem = { sizeof(mem) };
    GlobalMemoryStatusEx(&mem);
    systemMemoryMB = (mem.ullTotalPhys - mem.ullAvailPhys) / (1024 * 1024);
}

GameObject* ModuleEditor::createEmptyGameObject(const char* name, GameObject* parent)
{
    ModuleScene* scene = getActiveModuleScene();
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

    ModuleScene* scene = getActiveModuleScene();
    if (!scene) return;

    GameObject* grandparent = go->getParent();
    for (GameObject* child : go->getChildren()) child->setParent(grandparent);

    std::string name = go->getName();
    scene->destroyGameObject(go);
    log(("Deleted: " + name).c_str(), ImVec4(1.0f, 0.7f, 0.4f, 1.0f));
}

bool ModuleEditor::isChildOf(const GameObject* root, const GameObject* needle)
{
    if (!root || !needle) return false;
    if (root == needle)   return true;
    for (const GameObject* child : root->getChildren())
        if (isChildOf(child, needle)) return true;
    return false;
}

void ModuleEditor::debugDrawLights(ModuleScene* scene, float lightSize)
{
    if (!scene) return;
    auto v = [](const Vector3& vec) -> const float* { return &vec.x; };

    std::function<void(GameObject*)> visit = [&](GameObject* node)
        {
            if (!node || !node->isActive()) return;

            if (auto* dl = node->getComponent<ComponentDirectionalLight>(); dl && dl->enabled)
            {
                Vector3 pos = node->getTransform()->getGlobalMatrix().Translation();
                Vector3 dir = dl->direction; dir.Normalize();
                Vector3 end = pos + dir * lightSize * 2.0f;
                float hs = lightSize * 0.2f;
                dd::line(v(pos), v(end), dd::colors::Yellow);
                dd::line(v(pos + Vector3(-hs, 0, 0)), v(pos + Vector3(hs, 0, 0)), dd::colors::Yellow);
                dd::line(v(pos + Vector3(0, -hs, 0)), v(pos + Vector3(0, hs, 0)), dd::colors::Yellow);
                dd::line(v(pos + Vector3(0, 0, -hs)), v(pos + Vector3(0, 0, hs)), dd::colors::Yellow);
            }

            if (auto* pl = node->getComponent<ComponentPointLight>(); pl && pl->enabled)
            {
                Vector3 pos = node->getTransform()->getGlobalMatrix().Translation();
                float hs = lightSize * 0.2f;
                dd::sphere(v(pos), dd::colors::Cyan, pl->radius);
                dd::line(v(pos + Vector3(-hs, 0, 0)), v(pos + Vector3(hs, 0, 0)), dd::colors::Cyan);
                dd::line(v(pos + Vector3(0, -hs, 0)), v(pos + Vector3(0, hs, 0)), dd::colors::Cyan);
                dd::line(v(pos + Vector3(0, 0, -hs)), v(pos + Vector3(0, 0, hs)), dd::colors::Cyan);
            }

            if (auto* sl = node->getComponent<ComponentSpotLight>(); sl && sl->enabled)
            {
                Vector3 pos = node->getTransform()->getGlobalMatrix().Translation();
                Vector3 dir = sl->direction; dir.Normalize();
                float outerRad = tanf(sl->outerAngle * 0.0174532925f) * sl->radius;
                float innerRad = tanf(sl->innerAngle * 0.0174532925f) * sl->radius;
                Vector3 tip = pos + dir * sl->radius;

                Vector3 up = (fabsf(dir.y) < 0.99f) ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
                Vector3 right = dir.Cross(up); right.Normalize();
                up = right.Cross(dir); up.Normalize();

                const float twoPi = 6.28318530f;
                const int   segs = 8;
                for (int i = 0; i < segs; ++i)
                {
                    float a0 = (float)i / segs * twoPi;
                    float a1 = (float)(i + 1) / segs * twoPi;
                    Vector3 o0 = tip + (right * cosf(a0) + up * sinf(a0)) * outerRad;
                    Vector3 o1 = tip + (right * cosf(a1) + up * sinf(a1)) * outerRad;
                    Vector3 i0 = tip + (right * cosf(a0) + up * sinf(a0)) * innerRad;
                    Vector3 i1 = tip + (right * cosf(a1) + up * sinf(a1)) * innerRad;
                    dd::line(v(pos), v(o0), dd::colors::Orange);
                    dd::line(v(o0), v(o1), dd::colors::Orange);
                    dd::line(v(i0), v(i1), dd::colors::Yellow);
                }

                float hs = lightSize * 0.2f;
                dd::line(v(pos), v(tip), dd::colors::Orange);
                dd::line(v(pos + Vector3(-hs, 0, 0)), v(pos + Vector3(hs, 0, 0)), dd::colors::Orange);
                dd::line(v(pos + Vector3(0, -hs, 0)), v(pos + Vector3(0, hs, 0)), dd::colors::Orange);
                dd::line(v(pos + Vector3(0, 0, -hs)), v(pos + Vector3(0, 0, hs)), dd::colors::Orange);
            }

            for (auto* child : node->getChildren()) visit(child);
        };

    visit(scene->getRoot());
}