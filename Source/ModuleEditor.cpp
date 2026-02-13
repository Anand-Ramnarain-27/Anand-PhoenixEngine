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
#include "RenderPipelineTestScene.h"
#include "ModuleScene.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ModuleCamera.h"

#include <d3dx12.h>

ModuleEditor::ModuleEditor() {}
ModuleEditor::~ModuleEditor() {}

bool ModuleEditor::init()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();

    descTable = descriptors->allocTable();

    /*auto fs = app->getFileSystem();

    fs->Save("Library/test.txt", "Hello", 5);*/

    imguiPass = std::make_unique<ImGuiPass>(d3d12->getDevice(), d3d12->getHWnd(), descTable.getCPUHandle(), descTable.getGPUHandle());
    debugDrawPass = std::make_unique<DebugDrawPass>(d3d12->getDevice(), d3d12->getDrawCommandQueue(), false);
    viewportRT = std::make_unique<RenderTexture>("EditorViewport", DXGI_FORMAT_R8G8B8A8_UNORM, Vector4(0.0f, 0.0f, 0.0f, 1.0f), DXGI_FORMAT_D32_FLOAT, 1.0f);
    sceneManager = std::make_unique<SceneManager>();
    meshPipeline = std::make_unique<MeshPipeline>();

    if (!meshPipeline->init(d3d12->getDevice()))
    {
        return false;
    }

    availableScenes =
    {
        {
            "Render Pipeline Test",
            []() { return std::make_unique<RenderPipelineTestScene>(); }
        }
        // Later:
        // { "Lighting Demo", [](){ return std::make_unique<LightingScene>(); } },
    };
    selectedSceneIndex = 0;
    sceneManager->setScene(availableScenes[0].create(), app->getD3D12()->getDevice());

    log("[Editor] Active scene: RenderPipelineTestScene", ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
    log("[Editor] ModuleEditor initialized", ImVec4(0.6f, 1.0f, 0.6f, 1.0f));

    D3D12_QUERY_HEAP_DESC qDesc = {};
    qDesc.Count = 2;
    qDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

    app->getD3D12()->getDevice()->CreateQueryHeap(&qDesc, IID_PPV_ARGS(&gpuQueryHeap));

    D3D12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * 2);

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);

    app->getD3D12()->getDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&gpuReadbackBuffer));

    {
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(CameraConstants) + 255) & ~255); // Align to 256

        d3d12->getDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&cameraConstantBuffer)
        );

        cameraConstantBuffer->SetName(L"CameraCB");
    }

    // Object constant buffer
    {
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(ObjectConstants) + 255) & ~255);

        d3d12->getDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&objectConstantBuffer)
        );

        objectConstantBuffer->SetName(L"ObjectCB");
    }

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
            {
                sceneManager->onViewportResized(pendingViewportWidth, pendingViewportHeight);
            }
        }
        pendingViewportResize = false;
    }

    imguiPass->startFrame();

    if (sceneManager)
        sceneManager->update(app->getElapsedMilis() * 0.001f);


    updateFPS();

    drawDockspace();
    drawMenuBar();

    if (showViewport){
        drawViewport();
        drawViewportOverlay();
    }

    if (showHierarchy)   drawHierarchy();
    if (showInspector)   drawInspector();
    if (showConsole)     drawConsole();
    if (showPerformance) drawPerformanceWindow();
    if (showExercises)   drawExercises();
    if (showAssetBrowser) drawAssetBrowser();

    if (viewportRT && viewportSize.x > 4 && viewportSize.y > 4)
    {
        if (viewportSize.x != lastViewportSize.x ||
            viewportSize.y != lastViewportSize.y)
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

    ID3D12DescriptorHeap* heaps[] =
    {
        descriptors->getHeap()
    };
    cmd->SetDescriptorHeaps(1, heaps);

    if (viewportRT && viewportRT->isValid())
    {
        renderViewportToTexture(cmd);
    }

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->getBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    cmd->ResourceBarrier(1, &barrier);

    auto rtv = d3d12->getRenderTargetDescriptor();

    cmd->OMSetRenderTargets(1, &rtv, false, nullptr);

    float clear[] = { 0,0,0,1 };
    cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);

    imguiPass->record(cmd);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(d3d12->getBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    cmd->ResourceBarrier(1, &barrier);

    cmd->EndQuery(gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

    cmd->ResolveQueryData(gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, gpuReadbackBuffer.Get(), 0);

    cmd->Close();

    ID3D12CommandList* lists[] = { cmd };

    d3d12->getDrawCommandQueue()->ExecuteCommandLists(1, lists);

    UINT64* data = nullptr;

    gpuReadbackBuffer->Map(0, nullptr, (void**)&data);

    if (data)
    {
        UINT64 start = data[0];
        UINT64 end = data[1];

        UINT64 freq = 0;
        app->getD3D12()->getDrawCommandQueue()->GetTimestampFrequency(&freq);

        gpuFrameTimeMs =double(end - start) / double(freq) * 1000.0;
        gpuTimerReady = true;
        gpuReadbackBuffer->Unmap(0, nullptr);
    }

}

void ModuleEditor::renderViewportToTexture(ID3D12GraphicsCommandList* cmd)
{
    ModuleCamera* camera = app->getCamera();

    const UINT width = UINT(viewportSize.x);
    const UINT height = UINT(viewportSize.y);

    if (!viewportRT || width == 0 || height == 0)
        return;

    const Matrix& view = camera->getView();
    Matrix proj = ModuleCamera::getPerspectiveProj(float(width) / float(height));

    viewportRT->beginRender(cmd);

    BEGIN_EVENT(cmd, "Editor Viewport Pass");

    cmd->SetPipelineState(meshPipeline->getPSO());
    cmd->SetGraphicsRootSignature(meshPipeline->getRootSig());

    // Bind camera constant (b0) - 16 x 32-bit values = 4x4 matrix
    Matrix viewProj = (view * proj).Transpose();
    cmd->SetGraphicsRoot32BitConstants(0, 16, &viewProj, 0);

    // Bind world matrix (b1) - 16 x 32-bit values = 4x4 matrix
    Matrix world = Matrix::Identity.Transpose();
    cmd->SetGraphicsRoot32BitConstants(1, 16, &world, 0);

    if (sceneManager)
        sceneManager->render(cmd, *camera, width, height);

    debugDrawPass->record(cmd, width, height, view, proj);

    END_EVENT(cmd);

    viewportRT->endRender(cmd);
}

void ModuleEditor::log(const char* text, const ImVec4& color)
{
    console.push_back({ text, color });
}

void ModuleEditor::updateFPS()
{
    float fps = app->getFPS();

    fpsHistory[fpsIndex] = fps;
    fpsIndex = (fpsIndex + 1) % FPS_HISTORY;
}


void ModuleEditor::drawDockspace()
{
    ImGuiIO& io = ImGui::GetIO();

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    const ImGuiViewport* vp = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

    ImGui::Begin("MainDockspace", nullptr, flags);

    ImGui::PopStyleVar(2);

    ImGuiID dockID = ImGui::GetID("Dockspace");

    ImGui::DockSpace(dockID, ImVec2(0, 0));

    if (firstFrame)
    {
        firstFrame = false;

        ImGui::DockBuilderRemoveNode(dockID);
        ImGui::DockBuilderAddNode(dockID, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);

        ImGui::DockBuilderSetNodeSize(dockID, ImGui::GetMainViewport()->Size);

        ImGuiID left, right, bottom, center, rightTop, rightBottom;

        ImGui::DockBuilderSplitNode(dockID, ImGuiDir_Left, 0.2f, &left, &right);

        ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.25f, &bottom, &right);

        ImGui::DockBuilderSplitNode(right, ImGuiDir_Right, 0.3f, &rightTop, &center);

        ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, 0.3f, &rightBottom, &bottom);

        ImGui::DockBuilderDockWindow("Hierarchy", left);
        ImGui::DockBuilderDockWindow("Inspector", rightTop);  
        ImGui::DockBuilderDockWindow("Viewport", center);  
        ImGui::DockBuilderDockWindow("Console", bottom);  
        ImGui::DockBuilderDockWindow("Performance", bottom);  
        ImGui::DockBuilderDockWindow("Exercises", left);  
        ImGui::DockBuilderDockWindow("Asset Browser", bottom);

        ImGui::DockBuilderFinish(dockID);
    }

    ImGui::End();
}

void ModuleEditor::drawMenuBar()
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Exit"))
        {
            //app->quit();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy);
        ImGui::MenuItem("Inspector", nullptr, &showInspector);
        ImGui::MenuItem("Console", nullptr, &showConsole);
        ImGui::MenuItem("Viewport", nullptr, &showViewport);
        ImGui::MenuItem("Performance", nullptr, &showPerformance);
        ImGui::MenuItem("Exercises", nullptr, &showExercises);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene"))
    {
        if (ImGui::MenuItem("Play"))  sceneManager->play();
        if (ImGui::MenuItem("Pause")) sceneManager->pause();
        if (ImGui::MenuItem("Stop"))  sceneManager->stop();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Assets"))
    {
        if (ImGui::MenuItem("Import Duck Model (Test)"))
        {
            app->getAssets()->importAsset("Assets/Models/Duck/duck.gltf");  // Use ModuleAssets
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Show Asset Browser", "Ctrl+A"))
        {
            showAssetBrowser = !showAssetBrowser;
        }

        ImGui::Separator();

        ImGui::TextDisabled("Library Folder:");
        ImGui::TextDisabled(app->getFileSystem()->GetLibraryPath().c_str());

        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void ModuleEditor::drawHierarchy()
{
    ImGui::Begin("Hierarchy", &showHierarchy);

    if (!sceneManager) { ImGui::End(); return; }

    IScene* active = sceneManager->getActiveScene();
    if (!active) { ImGui::End(); return; }

    ModuleScene* scene = active->getModuleScene();
    if (!scene) { ImGui::End(); return; }

    drawHierarchyNode(scene->getRoot());

    ImGui::End();
}

void ModuleEditor::drawHierarchyNode(GameObject* go)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (go == selectedGameObject)
        flags |= ImGuiTreeNodeFlags_Selected;

    if (go->getChildren().empty())
        flags |= ImGuiTreeNodeFlags_Leaf;

    bool opened = ImGui::TreeNodeEx((void*)go, flags, go->getName().c_str());

    if (ImGui::IsItemClicked())
        selectedGameObject = go;

    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("GAMEOBJECT", &go, sizeof(GameObject*));

        ImGui::Text("Move %s", go->getName().c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload("GAMEOBJECT"))
        {
            GameObject* dropped = *(GameObject**)payload->Data;

            if (dropped != go)
                dropped->setParent(go);
        }

        ImGui::EndDragDropTarget();
    }

    if (opened)
    {
        for (auto* child : go->getChildren())
            drawHierarchyNode(child);

        ImGui::TreePop();
    }
}

void ModuleEditor::drawExercises()
{
    ImGui::Begin("Exercises");

    ImGui::Text("Available Scenes");
    ImGui::Separator();

    for (int i = 0; i < (int)availableScenes.size(); ++i)
    {
        bool selected = (i == selectedSceneIndex);

        if (ImGui::Selectable(availableScenes[i].name, selected))
        {
            if (i != selectedSceneIndex)
            {
                selectedSceneIndex = i;

                sceneManager->setScene(availableScenes[i].create(), app->getD3D12()->getDevice());

                log(("Switched to scene: " + std::string(availableScenes[i].name)).c_str(), ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
            }
        }
    }

    ImGui::Separator();

    if (selectedSceneIndex >= 0)
    {
        ImGui::Text("Active: %s", availableScenes[selectedSceneIndex].name);
    }

    ImGui::End();
}


void ModuleEditor::drawViewport()
{
    ImGui::Begin("Viewport");

    ImVec2 min = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();

    viewportPos = min;
    viewportSize = size; 

    ImGuiID frameID = ImGui::GetID("EditorViewportFrame");
    ImGui::BeginChildFrame(frameID, viewportSize, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (viewportRT && viewportRT->isValid() && !pendingViewportResize)
    {
        ImGui::Image((ImTextureID)viewportRT->getSrvHandle().ptr, viewportSize);
    }
    else
    {
        if (pendingViewportResize)
            ImGui::TextDisabled("Resizing viewport to %dx%d...",
                (int)viewportSize.x, (int)viewportSize.y);
        else if (!viewportRT)
            ImGui::TextDisabled("Viewport render texture not initialized");
        else if (!viewportRT->isValid())
            ImGui::TextDisabled("Viewport render texture invalid");
    }

    ImGui::EndChildFrame();
    ImGui::End();
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

    char buffer[256];
    strcpy_s(buffer, selectedGameObject->getName().c_str());

    if (ImGui::InputText("Name", buffer, 256))
        selectedGameObject->setName(buffer);

    if (ComponentTransform* t = selectedGameObject->getTransform())
    {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::DragFloat3("Position", &t->position.x, 0.1f))
                t->markDirty();

            if (ImGui::DragFloat3("Scale", &t->scale.x, 0.1f))
                t->markDirty();

            Vector3 euler = t->rotation.ToEuler();

            if (ImGui::DragFloat3("Rotation", &euler.x, 0.1f))
            {
                t->rotation = Quaternion::CreateFromYawPitchRoll(euler.y, euler.x, euler.z);
                t->markDirty();
            }
        }
    }
    ImGui::End();
}

void ModuleEditor::drawConsole()
{
    ImGui::Begin("Console");

    if (ImGui::Button("Clear"))
    {
        console.clear();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScrollConsole);

    ImGui::Separator();

    ImGui::BeginChild("ConsoleScroll");

    for (const auto& entry : console)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, entry.color);
        ImGui::TextUnformatted(entry.text.c_str());
        ImGui::PopStyleColor();
    }

    if (autoScrollConsole && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    ImGui::End();
}

void ModuleEditor::drawPerformanceWindow()
{
    if (!showPerformance)
        return;

    ImGui::Begin("FPS / Performance", &showPerformance);

    ImGui::Text("Performance");
    ImGui::Separator();

    ImGui::Text("FPS: %.1f", app->getFPS());
    ImGui::Text("CPU: %.2f ms", app->getAvgElapsedMs());

    if (gpuTimerReady)
        ImGui::Text("GPU: %.2f ms", gpuFrameTimeMs);

    ImGui::Separator();

    ImGui::Text("Memory");
    ImGui::Text("VRAM: %llu MB", gpuMemoryMB);
    ImGui::Text("RAM:  %llu MB", systemMemoryMB);

    ImGui::Separator();

    static float ordered[FPS_HISTORY];

    for (int i = 0; i < FPS_HISTORY; i++)
    {
        int index = (fpsIndex + i) % FPS_HISTORY;
        ordered[i] = fpsHistory[index];
    }

    float maxFPS = 0.0f;
    for (float v : ordered)
        if (v > maxFPS) maxFPS = v;

    if (maxFPS < 60.0f)
        maxFPS = 60.0f;

    ImGui::PlotLines("##FPSGraph", ordered, FPS_HISTORY, 0, nullptr, 0.0f, maxFPS * 1.1f, ImVec2(0, 120));

    ImGui::End();
}

void ModuleEditor::updateMemory()
{
    gpuMemoryMB = 0;
    systemMemoryMB = 0;

    ID3D12Device* device = app->getD3D12()->getDevice();

    if (device)
    {
        ComPtr<IDXGIDevice> dxgiDevice;
        device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));

        ComPtr<IDXGIAdapter> adapter;
        dxgiDevice->GetAdapter(&adapter);

        ComPtr<IDXGIAdapter3> adapter3;
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

    systemMemoryMB =
        (mem.ullTotalPhys - mem.ullAvailPhys) / (1024 * 1024);
}

void ModuleEditor::drawViewportOverlay()
{
    ImGuiWindow* window = ImGui::FindWindowByName("Viewport");

    if (!window)
        return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    ImVec2 pos = window->Pos;

    pos.x += 10;
    pos.y += 30;

    char buf[128];

    sprintf_s(buf, "FPS: %.1f | CPU: %.2f ms | GPU: %.2f ms", app->getFPS(), app->getAvgElapsedMs(), gpuFrameTimeMs);

    draw->AddText(pos, IM_COL32(0, 255, 0, 255),buf);
}

void ModuleEditor::drawAssetBrowser()
{
    if (!showAssetBrowser)
        return;

    ImGui::Begin("Asset Browser", &showAssetBrowser);

    ModuleAssets* assets = app->getAssets();

    // === IMPORT SECTION ===
    ImGui::SeparatorText("Import");

    if (ImGui::Button("Import Duck Model (Test)", ImVec2(-1, 0)))
    {
        assets->importAsset("Assets/Models/Duck/duck.gltf");
    }

    ImGui::Spacing();

    // Manual import path
    static char pathBuffer[256] = "Assets/Models/";
    ImGui::InputText("Path", pathBuffer, 256);
    ImGui::SameLine();
    if (ImGui::Button("Import"))
    {
        assets->importAsset(pathBuffer);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // === LIBRARY SECTION ===
    ImGui::SeparatorText("Imported Scenes");

    // Get list of imported scenes
    auto scenes = assets->getImportedScenes();

    if (scenes.empty())
    {
        ImGui::TextDisabled("No scenes imported yet.");
        ImGui::TextDisabled("Use the Import button above to import a model.");
    }
    else
    {
        for (const auto& sceneInfo : scenes)
        {
            ImGui::PushID(sceneInfo.name.c_str());

            // Show scene name with bullet
            ImGui::BulletText("%s", sceneInfo.name.c_str());

            // Show metadata
            ImGui::Indent();
            ImGui::TextDisabled("Meshes: %u | Materials: %u",
                sceneInfo.meshCount, sceneInfo.materialCount);

            // Load button
            if (ImGui::Button("Load in Scene"))
            {
                log(("Loading scene from library: " + sceneInfo.name).c_str(),
                    ImVec4(0.6f, 0.8f, 1.0f, 1.0f));

                // TODO: Actually load the scene
                // For now, just log it
            }

            ImGui::SameLine();

            // Show folder button
            if (ImGui::SmallButton("Show Folder"))
            {
                // TODO: Open folder in file explorer
                log(("Scene folder: " + sceneInfo.path).c_str(),
                    ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
            }

            ImGui::Unindent();
            ImGui::Spacing();

            ImGui::PopID();
        }
    }

    ImGui::Separator();

    // Show library path
    ImGui::TextDisabled("Library: %s",
        app->getFileSystem()->GetLibraryPath().c_str());

    ImGui::End();
}

void ModuleEditor::updateCameraConstants(const Matrix& view, const Matrix& proj)
{
    CameraConstants constants;
    constants.viewProj = (view * proj).Transpose(); // D3D expects row-major

    void* data = nullptr;
    cameraConstantBuffer->Map(0, nullptr, &data);
    memcpy(data, &constants, sizeof(CameraConstants));
    cameraConstantBuffer->Unmap(0, nullptr);
}