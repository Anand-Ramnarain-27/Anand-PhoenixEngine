#include "Globals.h"
#include "ImGuiPass.h"
#include "EditorColors.h"
#include "Application.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include <cstdio>

// Global font pointers — accessible to any panel that wants the mono face.
ImFont* g_fontUI   = nullptr;
ImFont* g_fontMono = nullptr;

// ---- Phoenix theme -------------------------------------------------------
// Defined here (not in a separate .cpp) so no extra project file is needed.
static void PhoenixTheme_Apply() {
    using namespace EditorColors;

    ImGuiStyle& s = ImGui::GetStyle();

    // Rounding / spacing
    s.WindowRounding    = 6.f;
    s.ChildRounding     = 4.f;
    s.FrameRounding     = 4.f;
    s.PopupRounding     = 6.f;
    s.ScrollbarRounding = 4.f;
    s.GrabRounding      = 4.f;
    s.TabRounding       = 4.f;

    s.WindowBorderSize  = 1.f;
    s.FrameBorderSize   = 0.f;
    s.PopupBorderSize   = 1.f;
    s.TabBorderSize     = 0.f;

    s.WindowPadding     = ImVec2(10.f, 8.f);
    s.FramePadding      = ImVec2(6.f,  3.f);
    s.ItemSpacing       = ImVec2(6.f,  4.f);
    s.ItemInnerSpacing  = ImVec2(4.f,  4.f);
    s.IndentSpacing     = 16.f;
    s.ScrollbarSize     = 10.f;
    s.GrabMinSize       = 10.f;

    // Colors
    ImVec4* c = s.Colors;

    c[ImGuiCol_Text]                     = Tx0;
    c[ImGuiCol_TextDisabled]             = Tx2;
    c[ImGuiCol_WindowBg]                 = Bg0;
    c[ImGuiCol_ChildBg]                  = Bg1;
    c[ImGuiCol_PopupBg]                  = ImVec4(0.122f, 0.122f, 0.149f, 0.98f);
    c[ImGuiCol_Border]                   = Line;
    c[ImGuiCol_BorderShadow]             = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]                  = Bg2;
    c[ImGuiCol_FrameBgHovered]           = Bg3;
    c[ImGuiCol_FrameBgActive]            = Bg4;
    c[ImGuiCol_TitleBg]                  = Bg0;
    c[ImGuiCol_TitleBgActive]            = ImVec4(0.102f, 0.102f, 0.133f, 1.f);
    c[ImGuiCol_TitleBgCollapsed]         = Bg0;
    c[ImGuiCol_MenuBarBg]                = ImVec4(0.118f, 0.118f, 0.141f, 1.f);
    c[ImGuiCol_ScrollbarBg]              = Bg1;
    c[ImGuiCol_ScrollbarGrab]            = Bg3;
    c[ImGuiCol_ScrollbarGrabHovered]     = Bg4;
    c[ImGuiCol_ScrollbarGrabActive]      = Acc;
    c[ImGuiCol_CheckMark]                = Acc;
    c[ImGuiCol_SliderGrab]               = Acc;
    c[ImGuiCol_SliderGrabActive]         = Acc2;
    c[ImGuiCol_Button]                   = Bg3;
    c[ImGuiCol_ButtonHovered]            = Bg4;
    c[ImGuiCol_ButtonActive]             = ImVec4(0.235f, 0.157f, 0.349f, 1.f);
    c[ImGuiCol_Header]                   = AccDim;
    c[ImGuiCol_HeaderHovered]            = ImVec4(0.690f, 0.482f, 0.941f, 0.28f);
    c[ImGuiCol_HeaderActive]             = ImVec4(0.690f, 0.482f, 0.941f, 0.45f);
    c[ImGuiCol_Separator]                = Line;
    c[ImGuiCol_SeparatorHovered]         = Acc;
    c[ImGuiCol_SeparatorActive]          = Acc2;
    c[ImGuiCol_ResizeGrip]               = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered]        = AccDim;
    c[ImGuiCol_ResizeGripActive]         = Acc;
    c[ImGuiCol_Tab]                      = Bg0;
    c[ImGuiCol_TabHovered]               = Bg3;
    c[ImGuiCol_TabSelected]              = Bg1;
    c[ImGuiCol_TabSelectedOverline]      = Acc;
    c[ImGuiCol_TabDimmed]                = Bg0;
    c[ImGuiCol_TabDimmedSelected]        = Bg1;
    c[ImGuiCol_TabDimmedSelectedOverline]= ImVec4(0, 0, 0, 0);
    c[ImGuiCol_DockingPreview]           = AccDim;
    c[ImGuiCol_DockingEmptyBg]           = BgVoid;
    c[ImGuiCol_PlotLines]                = Acc;
    c[ImGuiCol_PlotLinesHovered]         = Acc2;
    c[ImGuiCol_PlotHistogram]            = Ok;
    c[ImGuiCol_PlotHistogramHovered]     = Warn;
    c[ImGuiCol_TableHeaderBg]            = Bg2;
    c[ImGuiCol_TableBorderStrong]        = Line2;
    c[ImGuiCol_TableBorderLight]         = Line;
    c[ImGuiCol_TableRowBg]               = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]            = ImVec4(1, 1, 1, 0.025f);
    c[ImGuiCol_TextSelectedBg]           = ImVec4(0.690f, 0.482f, 0.941f, 0.35f);
    c[ImGuiCol_DragDropTarget]           = Acc;
    c[ImGuiCol_NavHighlight]             = Acc;
    c[ImGuiCol_NavWindowingHighlight]    = Acc;
    c[ImGuiCol_NavWindowingDimBg]        = ImVec4(0.f, 0.f, 0.f, 0.4f);
    c[ImGuiCol_ModalWindowDimBg]         = ImVec4(0.f, 0.f, 0.f, 0.5f);
}
// -------------------------------------------------------------------------

ImGuiPass::ImGuiPass(ID3D12Device2* device, HWND hWnd,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuTextHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE gpuTextHandle)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    if (!cpuTextHandle.ptr || !gpuTextHandle.ptr) {
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap));
        heap->SetName(L"ImGui Descriptor Heap");
        cpuTextHandle = heap->GetCPUDescriptorHandleForHeapStart();
        gpuTextHandle = heap->GetGPUDescriptorHandleForHeapStart();
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    PhoenixTheme_Apply();

    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX12_Init(device, FRAMES_IN_FLIGHT, DXGI_FORMAT_R8G8B8A8_UNORM,
        nullptr, cpuTextHandle, gpuTextHandle);

    // Font loading — try bundled fonts first, fall back to Windows system fonts.
    auto tryLoad = [&](const char* path, float size) -> ImFont* {
        FILE* fp = nullptr;
        fopen_s(&fp, path, "r");
        if (fp) { fclose(fp); return io.Fonts->AddFontFromFileTTF(path, size); }
        return nullptr;
    };

    g_fontUI = tryLoad("Assets/Fonts/IBMPlexSans-Regular.ttf", 12.f);
    if (!g_fontUI) g_fontUI = tryLoad("Assets/Fonts/IBMPlexSans.ttf", 12.f);
    if (!g_fontUI) g_fontUI = tryLoad("c:\\Windows\\Fonts\\segoeui.ttf", 13.f);
    if (!g_fontUI) g_fontUI = io.Fonts->AddFontDefault();

    g_fontMono = tryLoad("Assets/Fonts/JetBrainsMono-Regular.ttf", 11.f);
    if (!g_fontMono) g_fontMono = tryLoad("Assets/Fonts/JetBrainsMono.ttf", 11.f);
    if (!g_fontMono) g_fontMono = tryLoad("c:\\Windows\\Fonts\\consola.ttf", 11.f);
    if (!g_fontMono) g_fontMono = tryLoad("c:\\Windows\\Fonts\\cour.ttf", 11.f);
    if (!g_fontMono) g_fontMono = io.Fonts->AddFontDefault();

    io.Fonts->Build();
}

ImGuiPass::~ImGuiPass() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiPass::startFrame() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiPass::record(ID3D12GraphicsCommandList* commandList) {
    BEGIN_EVENT(commandList, "ImGui Pass");
    ImGui::Render();
    if (heap) {
        ID3D12DescriptorHeap* heaps[] = { heap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);
    }
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
    END_EVENT(commandList);
}
