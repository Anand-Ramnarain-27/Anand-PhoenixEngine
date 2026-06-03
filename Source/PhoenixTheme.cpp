#include "Globals.h"
#include "EditorColors.h"
#include <imgui.h>
#include <cstdio>

// Helper: convert #RRGGBB hex literal to ImVec4 (alpha=1).
static constexpr ImVec4 hex(float r, float g, float b, float a = 1.f) { return { r, g, b, a }; }

void PhoenixTheme_Apply() {
    using namespace EditorColors;

    ImGuiStyle& s = ImGui::GetStyle();

    // ---- Rounding / spacing ----
    s.WindowRounding     = 6.f;
    s.ChildRounding      = 4.f;
    s.FrameRounding      = 4.f;
    s.PopupRounding      = 6.f;
    s.ScrollbarRounding  = 4.f;
    s.GrabRounding       = 4.f;
    s.TabRounding        = 4.f;

    s.WindowBorderSize   = 1.f;
    s.FrameBorderSize    = 0.f;
    s.PopupBorderSize    = 1.f;
    s.TabBorderSize      = 0.f;

    s.WindowPadding      = { 10.f, 8.f };
    s.FramePadding       = { 6.f, 3.f };
    s.ItemSpacing        = { 6.f, 4.f };
    s.ItemInnerSpacing   = { 4.f, 4.f };
    s.IndentSpacing      = 16.f;
    s.ScrollbarSize      = 10.f;
    s.GrabMinSize        = 10.f;
    s.TabMinWidthForCloseButton = 0.f;

    // ---- Colors ----
    ImVec4* c = s.Colors;

    c[ImGuiCol_Text]                  = Tx0;
    c[ImGuiCol_TextDisabled]          = Tx2;
    c[ImGuiCol_WindowBg]              = Bg0;
    c[ImGuiCol_ChildBg]               = Bg1;
    c[ImGuiCol_PopupBg]               = { 0.122f, 0.122f, 0.149f, 0.98f };
    c[ImGuiCol_Border]                = Line;
    c[ImGuiCol_BorderShadow]          = { 0, 0, 0, 0 };
    c[ImGuiCol_FrameBg]               = Bg2;
    c[ImGuiCol_FrameBgHovered]        = Bg3;
    c[ImGuiCol_FrameBgActive]         = Bg4;
    c[ImGuiCol_TitleBg]               = Bg0;
    c[ImGuiCol_TitleBgActive]         = { 0.102f, 0.102f, 0.133f, 1.f };
    c[ImGuiCol_TitleBgCollapsed]      = Bg0;
    c[ImGuiCol_MenuBarBg]             = { 0.118f, 0.118f, 0.141f, 1.f };
    c[ImGuiCol_ScrollbarBg]           = Bg1;
    c[ImGuiCol_ScrollbarGrab]         = Bg3;
    c[ImGuiCol_ScrollbarGrabHovered]  = Bg4;
    c[ImGuiCol_ScrollbarGrabActive]   = Acc;
    c[ImGuiCol_CheckMark]             = Acc;
    c[ImGuiCol_SliderGrab]            = Acc;
    c[ImGuiCol_SliderGrabActive]      = Acc2;
    c[ImGuiCol_Button]                = Bg3;
    c[ImGuiCol_ButtonHovered]         = Bg4;
    c[ImGuiCol_ButtonActive]          = { 0.235f, 0.157f, 0.349f, 1.f };
    c[ImGuiCol_Header]                = AccDim;
    c[ImGuiCol_HeaderHovered]         = { 0.690f, 0.482f, 0.941f, 0.28f };
    c[ImGuiCol_HeaderActive]          = { 0.690f, 0.482f, 0.941f, 0.45f };
    c[ImGuiCol_Separator]             = Line;
    c[ImGuiCol_SeparatorHovered]      = Acc;
    c[ImGuiCol_SeparatorActive]       = Acc2;
    c[ImGuiCol_ResizeGrip]            = { 0, 0, 0, 0 };
    c[ImGuiCol_ResizeGripHovered]     = AccDim;
    c[ImGuiCol_ResizeGripActive]      = Acc;
    c[ImGuiCol_Tab]                   = Bg0;
    c[ImGuiCol_TabHovered]            = Bg3;
    c[ImGuiCol_TabSelected]           = Bg1;
    c[ImGuiCol_TabSelectedOverline]   = Acc;
    c[ImGuiCol_TabDimmed]             = Bg0;
    c[ImGuiCol_TabDimmedSelected]     = Bg1;
    c[ImGuiCol_TabDimmedSelectedOverline] = { 0, 0, 0, 0 };
    c[ImGuiCol_DockingPreview]        = AccDim;
    c[ImGuiCol_DockingEmptyBg]        = BgVoid;
    c[ImGuiCol_PlotLines]             = Acc;
    c[ImGuiCol_PlotLinesHovered]      = Acc2;
    c[ImGuiCol_PlotHistogram]         = Ok;
    c[ImGuiCol_PlotHistogramHovered]  = Warn;
    c[ImGuiCol_TableHeaderBg]         = Bg2;
    c[ImGuiCol_TableBorderStrong]     = Line2;
    c[ImGuiCol_TableBorderLight]      = Line;
    c[ImGuiCol_TableRowBg]            = { 0, 0, 0, 0 };
    c[ImGuiCol_TableRowBgAlt]         = { 1, 1, 1, 0.025f };
    c[ImGuiCol_TextSelectedBg]        = { 0.690f, 0.482f, 0.941f, 0.35f };
    c[ImGuiCol_DragDropTarget]        = Acc;
    c[ImGuiCol_NavHighlight]          = Acc;
    c[ImGuiCol_NavWindowingHighlight]  = Acc;
    c[ImGuiCol_NavWindowingDimBg]     = { 0.f, 0.f, 0.f, 0.4f };
    c[ImGuiCol_ModalWindowDimBg]      = { 0.f, 0.f, 0.f, 0.5f };
}
