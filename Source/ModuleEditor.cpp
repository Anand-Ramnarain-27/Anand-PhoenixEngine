#include "Globals.h"
#include "ModuleEditor.h"

#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleCamera.h"  
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "RenderPipelineTestScene.h"

#include "ImGuiPass.h"
#include <imgui.h>
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"

#include <d3dx12.h>

ModuleEditor::ModuleEditor()
{
}

ModuleEditor::~ModuleEditor()
{
}

bool ModuleEditor::init()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();

    descTable = descriptors->allocTable();

    imguiPass = std::make_unique<ImGuiPass>(
        d3d12->getDevice(),
        d3d12->getHWnd(),
        descTable.getCPUHandle(),
        descTable.getGPUHandle()
    );

    debugDrawPass = std::make_unique<DebugDrawPass>(
        d3d12->getDevice(),
        d3d12->getDrawCommandQueue(),
        false
    );

    viewportRT = std::make_unique<RenderTexture>(
        "EditorViewport",
        DXGI_FORMAT_R8G8B8A8_UNORM,
        Vector4(0.0f, 0.0f, 0.0f, 1.0f),
        DXGI_FORMAT_D32_FLOAT,
        1.0f
    );

    sceneManager = std::make_unique<SceneManager>();

    availableScenes =
    {
        {
            "Render Pipeline Test",
            []() { return std::make_unique<RenderPipelineTestScene>(); }
        }
        // Later:
        // { "Lighting Demo", [](){ return std::make_unique<LightingScene>(); } },
        // { "PBR Materials", [](){ return std::make_unique<PBRScene>(); } }
    };
    selectedSceneIndex = 0;
    sceneManager->setScene(
        availableScenes[0].create(),
        app->getD3D12()->getDevice()
    );


    log(
        "[Editor] Active scene: RenderPipelineTestScene",
        ImVec4(0.6f, 0.8f, 1.0f, 1.0f)
    );


    log("[Editor] ModuleEditor initialized", ImVec4(0.6f, 1.0f, 0.6f, 1.0f));

    D3D12_QUERY_HEAP_DESC qDesc = {};
    qDesc.Count = 2;
    qDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

    app->getD3D12()->getDevice()->CreateQueryHeap(
        &qDesc,
        IID_PPV_ARGS(&gpuQueryHeap)
    );

    D3D12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * 2);

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);

    app->getD3D12()->getDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&gpuReadbackBuffer)
    );

    return true;
}

bool ModuleEditor::cleanUp()
{
    imguiPass.reset();
    debugDrawPass.reset();
    viewportRT.reset();

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

            viewportRT->resize(
                pendingViewportWidth,
                pendingViewportHeight
            );

            if (sceneManager)
            {
                sceneManager->onViewportResized(
                    pendingViewportWidth,
                    pendingViewportHeight
                );
            }
        }

        pendingViewportResize = false;
    }

    imguiPass->startFrame();

    if (sceneManager)
    {
        float deltaTime = app->getElapsedMilis() * 0.001f;
        sceneManager->update(deltaTime);
    }

    updateFPS();

    drawDockspace();
    drawMenuBar();

    if (showEditor)
    {
        drawEditorPanel();
        drawExerciseList();
    }

    drawViewport();
    drawConsole();
    drawFPSWindow();
    drawViewportOverlay();

    if (viewportRT && viewportSize.x > 0 && viewportSize.y > 0)
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

    cmd->EndQuery(gpuQueryHeap.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP,
        0);

    ID3D12DescriptorHeap* heaps[] =
    {
        descriptors->getHeap()
    };

    cmd->SetDescriptorHeaps(1, heaps);

    if (viewportRT && viewportRT->isValid())
    {
        renderViewportToTexture(cmd);
    }

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );

    cmd->ResourceBarrier(1, &barrier);

    auto rtv = d3d12->getRenderTargetDescriptor();

    cmd->OMSetRenderTargets(1, &rtv, false, nullptr);

    float clear[] = { 0,0,0,1 };
    cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);

    imguiPass->record(cmd);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );

    cmd->ResourceBarrier(1, &barrier);

    cmd->EndQuery(gpuQueryHeap.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP,
        1);

    cmd->ResolveQueryData(
        gpuQueryHeap.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP,
        0,
        2,
        gpuReadbackBuffer.Get(),
        0
    );

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
        app->getD3D12()->getDrawCommandQueue()
            ->GetTimestampFrequency(&freq);

        gpuFrameTimeMs =
            double(end - start) / double(freq) * 1000.0;

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
    Matrix proj = ModuleCamera::getPerspectiveProj(
        float(width) / float(height)
    );

    viewportRT->beginRender(cmd);

    BEGIN_EVENT(cmd, "Editor Viewport Pass");

    if (sceneManager)
    {
        sceneManager->render(
            cmd,
            *camera,
            width,
            height
        );
    }

    debugDrawPass->record(
        cmd,
        width,
        height,
        view,
        proj
    );

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
        ImGui::DockBuilderAddNode(
            dockID,
            ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode
        );

        ImGui::DockBuilderSetNodeSize(dockID, ImGui::GetMainViewport()->Size);

        ImGuiID dockLeft;
        ImGuiID dockMain;
        ImGuiID dockBottom;

        ImGui::DockBuilderSplitNode(
            dockID,
            ImGuiDir_Left,
            0.22f,      
            &dockLeft,
            &dockMain
        );

        ImGui::DockBuilderSplitNode(
            dockMain,
            ImGuiDir_Down,
            0.28f,        
            &dockBottom,
            &dockMain
        );

        ImGui::DockBuilderDockWindow("Editor", dockLeft);
        ImGui::DockBuilderDockWindow("Exercises", dockLeft);

        ImGui::DockBuilderDockWindow("Viewport", dockMain);

        ImGui::DockBuilderDockWindow("Console", dockBottom);
        ImGui::DockBuilderDockWindow("FPS / Performance", dockBottom);

        ImGui::DockBuilderFinish(dockID);
    }



    ImGui::End();
}

void ModuleEditor::drawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
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
            ImGui::MenuItem("Show Editor", nullptr, &showEditor);
            ImGui::MenuItem("Show FPS", nullptr, &showFPSWindow);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scene"))
        {
            bool playing = sceneManager->isPlaying();

            if (ImGui::MenuItem("Play", nullptr, false, !playing))
                sceneManager->play();

            if (ImGui::MenuItem("Pause", nullptr, false, playing))
                sceneManager->pause();

            if (ImGui::MenuItem("Stop"))
                sceneManager->stop();

            ImGui::EndMenu();
        }



        if (ImGui::BeginMenu("Help"))
        {
            ImGui::Text("Phoenix Editor");
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void ModuleEditor::drawEditorPanel()
{
    ImGui::Begin("Editor");

    ImGui::Text("Editor Tools");

    ImGui::Separator();

    ImGui::Checkbox("Show Editor", &showEditor);

    ImGui::Separator();

    ImGui::Text("Transform");
    ImGui::Text("Lighting");
    ImGui::Text("Materials");

    ImGui::Separator();

    ImGui::Text("Status");
    ImGui::Text("FPS: %.1f", app->getFPS());

    ImGui::End();
}

void ModuleEditor::drawExerciseList()
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

                sceneManager->setScene(
                    availableScenes[i].create(),
                    app->getD3D12()->getDevice()
                );

                log(
                    ("Switched to scene: " +
                        std::string(availableScenes[i].name)).c_str(),
                    ImVec4(0.6f, 0.8f, 1.0f, 1.0f)
                );
            }
        }
    }

    ImGui::Separator();

    if (selectedSceneIndex >= 0)
    {
        ImGui::Text(
            "Active: %s",
            availableScenes[selectedSceneIndex].name
        );
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

    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    ImGuiID frameID = ImGui::GetID("EditorViewportFrame");
    ImGui::BeginChildFrame(
        frameID,
        viewportSize,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    if (pendingViewportResize || !viewportRT || !viewportRT->isValid())
    {
        ImGui::TextDisabled("Resizing viewport...");
    }
    else
    {
        ImGui::Image(
            (ImTextureID)viewportRT->getSrvHandle().ptr,
            viewportSize
        );
    }

    ImGui::EndChildFrame();
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

void ModuleEditor::drawFPSWindow()
{
    if (!showFPSWindow)
        return;

    ImGui::Begin("FPS / Performance", &showFPSWindow);

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

    ImGui::PlotLines(
        "##FPSGraph",
        ordered,
        FPS_HISTORY,
        0,
        nullptr,
        0.0f,
        maxFPS * 1.1f,
        ImVec2(0, 120)
    );

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

            adapter3->QueryVideoMemoryInfo(
                0,
                DXGI_MEMORY_SEGMENT_GROUP_LOCAL,
                &info
            );

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

    sprintf_s(buf,
        "FPS: %.1f | CPU: %.2f ms | GPU: %.2f ms",
        app->getFPS(),
        app->getAvgElapsedMs(),
        gpuFrameTimeMs
    );

    draw->AddText(
        pos,
        IM_COL32(0, 255, 0, 255),
        buf
    );
}

