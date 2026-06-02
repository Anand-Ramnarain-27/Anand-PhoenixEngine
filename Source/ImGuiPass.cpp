#include "Globals.h"
#include "ImGuiPass.h"

#include "Application.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"


ImGuiPass::ImGuiPass(ID3D12Device2* device, HWND hWnd, D3D12_CPU_DESCRIPTOR_HANDLE cpuTextHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuTextHandle){

    // It's not optimal but makes ImGuiPass independent from ModuleDescriptor slides
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    if (!cpuTextHandle.ptr || !gpuTextHandle.ptr)
    {
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap));
        heap->SetName(L"ImGui Descriptor Heap");

        cpuTextHandle = heap->GetCPUDescriptorHandleForHeapStart();
        gpuTextHandle = heap->GetGPUDescriptorHandleForHeapStart();
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Phoenix Engine dark theme
    {
        auto& c = ImGui::GetStyle().Colors;
        c[ImGuiCol_Text]              = ImVec4(0.906f, 0.906f, 0.925f, 1.f);
        c[ImGuiCol_TextDisabled]      = ImVec4(0.443f, 0.443f, 0.486f, 1.f);
        c[ImGuiCol_WindowBg]          = ImVec4(0.078f, 0.078f, 0.090f, 1.f);
        c[ImGuiCol_ChildBg]           = ImVec4(0.102f, 0.102f, 0.118f, 1.f);
        c[ImGuiCol_PopupBg]           = ImVec4(0.102f, 0.102f, 0.118f, 0.98f);
        c[ImGuiCol_Border]            = ImVec4(0.173f, 0.173f, 0.200f, 1.f);
        c[ImGuiCol_BorderShadow]      = ImVec4(0.f,    0.f,    0.f,    0.f);
        c[ImGuiCol_FrameBg]           = ImVec4(0.129f, 0.129f, 0.149f, 1.f);
        c[ImGuiCol_FrameBgHovered]    = ImVec4(0.165f, 0.165f, 0.192f, 1.f);
        c[ImGuiCol_FrameBgActive]     = ImVec4(0.204f, 0.204f, 0.239f, 1.f);
        c[ImGuiCol_TitleBg]           = ImVec4(0.078f, 0.078f, 0.090f, 1.f);
        c[ImGuiCol_TitleBgActive]     = ImVec4(0.102f, 0.102f, 0.118f, 1.f);
        c[ImGuiCol_TitleBgCollapsed]  = ImVec4(0.078f, 0.078f, 0.090f, 1.f);
        c[ImGuiCol_MenuBarBg]         = ImVec4(0.055f, 0.055f, 0.063f, 1.f);
        c[ImGuiCol_ScrollbarBg]       = ImVec4(0.078f, 0.078f, 0.090f, 1.f);
        c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.165f, 0.165f, 0.192f, 1.f);
        c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.204f, 0.204f, 0.239f, 1.f);
        c[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.690f, 0.482f, 0.941f, 1.f);
        c[ImGuiCol_CheckMark]         = ImVec4(0.690f, 0.482f, 0.941f, 1.f);
        c[ImGuiCol_SliderGrab]        = ImVec4(0.690f, 0.482f, 0.941f, 1.f);
        c[ImGuiCol_SliderGrabActive]  = ImVec4(0.800f, 0.600f, 1.000f, 1.f);
        c[ImGuiCol_Button]            = ImVec4(0.165f, 0.165f, 0.192f, 1.f);
        c[ImGuiCol_ButtonHovered]     = ImVec4(0.204f, 0.204f, 0.239f, 1.f);
        c[ImGuiCol_ButtonActive]      = ImVec4(0.690f, 0.482f, 0.941f, 1.f);
        c[ImGuiCol_Header]            = ImVec4(0.690f, 0.482f, 0.941f, 0.20f);
        c[ImGuiCol_HeaderHovered]     = ImVec4(0.690f, 0.482f, 0.941f, 0.35f);
        c[ImGuiCol_HeaderActive]      = ImVec4(0.690f, 0.482f, 0.941f, 0.50f);
        c[ImGuiCol_Separator]         = ImVec4(0.173f, 0.173f, 0.200f, 1.f);
        c[ImGuiCol_SeparatorHovered]  = ImVec4(0.690f, 0.482f, 0.941f, 0.60f);
        c[ImGuiCol_SeparatorActive]   = ImVec4(0.690f, 0.482f, 0.941f, 1.f);
        c[ImGuiCol_ResizeGrip]        = ImVec4(0.690f, 0.482f, 0.941f, 0.20f);
        c[ImGuiCol_ResizeGripHovered] = ImVec4(0.690f, 0.482f, 0.941f, 0.60f);
        c[ImGuiCol_ResizeGripActive]  = ImVec4(0.690f, 0.482f, 0.941f, 1.f);
        c[ImGuiCol_Tab]               = ImVec4(0.102f, 0.102f, 0.118f, 1.f);
        c[ImGuiCol_TabHovered]        = ImVec4(0.204f, 0.204f, 0.239f, 1.f);
        c[ImGuiCol_TabActive]         = ImVec4(0.165f, 0.165f, 0.192f, 1.f);
        c[ImGuiCol_TabUnfocused]      = ImVec4(0.102f, 0.102f, 0.118f, 1.f);
        c[ImGuiCol_TabUnfocusedActive]= ImVec4(0.129f, 0.129f, 0.149f, 1.f);
        c[ImGuiCol_DockingPreview]    = ImVec4(0.690f, 0.482f, 0.941f, 0.70f);
        c[ImGuiCol_DockingEmptyBg]    = ImVec4(0.043f, 0.043f, 0.051f, 1.f);
        c[ImGuiCol_PlotLines]         = ImVec4(0.690f, 0.482f, 0.941f, 1.f);
        c[ImGuiCol_PlotLinesHovered]  = ImVec4(0.800f, 0.600f, 1.f,    1.f);
        c[ImGuiCol_PlotHistogram]     = ImVec4(0.690f, 0.482f, 0.941f, 1.f);
        c[ImGuiCol_PlotHistogramHovered]= ImVec4(0.800f, 0.600f, 1.f,  1.f);
        c[ImGuiCol_TableHeaderBg]     = ImVec4(0.102f, 0.102f, 0.118f, 1.f);
        c[ImGuiCol_TableBorderLight]  = ImVec4(0.173f, 0.173f, 0.200f, 1.f);
        c[ImGuiCol_TableBorderStrong] = ImVec4(0.204f, 0.204f, 0.239f, 1.f);
        c[ImGuiCol_TextSelectedBg]    = ImVec4(0.690f, 0.482f, 0.941f, 0.35f);
        c[ImGuiCol_DragDropTarget]    = ImVec4(0.690f, 0.482f, 0.941f, 0.90f);
        c[ImGuiCol_NavHighlight]      = ImVec4(0.690f, 0.482f, 0.941f, 1.f);

        auto& s = ImGui::GetStyle();
        s.WindowRounding    = 4.f;
        s.ChildRounding     = 4.f;
        s.FrameRounding     = 3.f;
        s.PopupRounding     = 4.f;
        s.ScrollbarRounding = 4.f;
        s.GrabRounding      = 3.f;
        s.TabRounding       = 4.f;
        s.FramePadding      = ImVec2(8.f, 4.f);
        s.ItemSpacing       = ImVec2(8.f, 4.f);
        s.ItemInnerSpacing  = ImVec2(4.f, 4.f);
        s.IndentSpacing     = 16.f;
        s.ScrollbarSize     = 12.f;
        s.GrabMinSize       = 8.f;
        s.WindowBorderSize  = 1.f;
        s.ChildBorderSize   = 1.f;
        s.PopupBorderSize   = 1.f;
        s.FrameBorderSize   = 0.f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX12_Init(device, FRAMES_IN_FLIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, nullptr, cpuTextHandle, gpuTextHandle);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !

    FILE* fp = fopen("c:\\Windows\\Fonts\\segoeui.ttf", "r");
    if (fp)
    {
        fclose(fp);
        io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    }
    else
    {
        io.Fonts->AddFontDefault();
    }

    io.Fonts->Build();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);
}

ImGuiPass::~ImGuiPass(){
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiPass::startFrame(){
    // imgui new frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();

    ImGui::NewFrame();

    // imgui commands
    //ImGui::ShowDemoWindow();
}

void ImGuiPass::record(ID3D12GraphicsCommandList* commandList){
    BEGIN_EVENT(commandList, "ImGui Pass");

    ImGui::Render();

    // It's not optimal but makes ImGuiPass independent from ModuleDescriptor slides

    if (heap)
    {
        ID3D12DescriptorHeap* descriptorHeaps[] = { heap.Get() };
        commandList->SetDescriptorHeaps(1, descriptorHeaps);
    }

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

    END_EVENT(commandList);
}
