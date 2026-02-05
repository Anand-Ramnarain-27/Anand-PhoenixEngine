#include "Globals.h"
#include "ModuleEditor.h"

#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"

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

    log("[Editor] ModuleEditor initialized", ImVec4(0.6f, 1.0f, 0.6f, 1.0f));

    return true;
}

bool ModuleEditor::cleanUp()
{
    imguiPass.reset();
    return true;
}

void ModuleEditor::preRender()
{
    imguiPass->startFrame();

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
}

void ModuleEditor::render()
{
    ModuleD3D12* d3d12 = app->getD3D12();
    ModuleShaderDescriptors* descriptors = app->getShaderDescriptors();

    ID3D12GraphicsCommandList* cmd = d3d12->getCommandList();

    cmd->Reset(d3d12->getCommandAllocator(), nullptr);

    ID3D12DescriptorHeap* heaps[] =
    {
        descriptors->getHeap()
    };

    cmd->SetDescriptorHeaps(1, heaps);

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

    cmd->Close();

    ID3D12CommandList* lists[] = { cmd };

    d3d12->getDrawCommandQueue()->ExecuteCommandLists(1, lists);
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

    ImGui::Text("Available Exercises");
    ImGui::Separator();

    ImGui::Selectable("Exercise 1");
    ImGui::Selectable("Exercise 2");
    ImGui::Selectable("Exercise 3");
    ImGui::Selectable("Render To Texture");

    ImGui::Separator();

    ImGui::Text("Selected: None");

    ImGui::End();
}

void ModuleEditor::drawViewport()
{
    ImGui::Begin("Viewport");

    ImVec2 min = ImGui::GetWindowContentRegionMin();
    ImVec2 max = ImGui::GetWindowContentRegionMax();

    viewportPos = ImGui::GetWindowPos();
    viewportSize = ImVec2(max.x - min.x, max.y - min.y);

    ImGui::Text("Scene View");

    ImGui::Separator();

    ImVec2 size = ImGui::GetContentRegionAvail();

    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetCursorScreenPos(),
        ImVec2(
            ImGui::GetCursorScreenPos().x + size.x,
            ImGui::GetCursorScreenPos().y + size.y
        ),
        IM_COL32(0, 0, 0, 255)
    );

    ImGui::InvisibleButton("viewport", size);

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

    float fps = app->getFPS();
    float ms = app->getAvgElapsedMs();

    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame Time: %.2f ms", ms);

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
