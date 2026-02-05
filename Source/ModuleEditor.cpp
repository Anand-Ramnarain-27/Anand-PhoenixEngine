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

    drawDockspace();
    drawMenuBar();

    if (showEditor)
    {
        drawEditorPanel();
        drawExerciseList();
    }

    drawViewport();
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

    // Backbuffer -> RT
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        d3d12->getBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );

    cmd->ResourceBarrier(1, &barrier);

    auto rtv = d3d12->getRenderTargetDescriptor();

    cmd->OMSetRenderTargets(1, &rtv, false, nullptr);

    // Clear black
    float clear[] = { 0,0,0,1 };
    cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);

    // Render ImGui
    imguiPass->record(cmd);

    // RT -> Present
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
        ImGui::DockBuilderAddNode(dockID, ImGuiDockNodeFlags_DockSpace);

        ImGuiID left, right;

        ImGui::DockBuilderSplitNode(
            dockID,
            ImGuiDir_Left,
            0.25f,
            &left,
            &right
        );

        ImGui::DockBuilderDockWindow("Editor", left);
        ImGui::DockBuilderDockWindow("Exercises", left);
        ImGui::DockBuilderDockWindow("Viewport", right);

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

    // Fake black viewport
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


