#pragma once
#include <imgui.h>

namespace EditorColors {
    // Background layers
    inline constexpr ImVec4 Bg0{ 0.078f, 0.078f, 0.090f, 1.f };   // #141417 WindowBg
    inline constexpr ImVec4 Bg1{ 0.102f, 0.102f, 0.118f, 1.f };   // #1a1a1e panel
    inline constexpr ImVec4 Bg2{ 0.129f, 0.129f, 0.149f, 1.f };   // #212126 FrameBg
    inline constexpr ImVec4 Bg3{ 0.165f, 0.165f, 0.192f, 1.f };   // #2a2a31 Button
    inline constexpr ImVec4 Bg4{ 0.204f, 0.204f, 0.239f, 1.f };   // #34343d ButtonHovered
    inline constexpr ImVec4 Line{ 0.173f, 0.173f, 0.200f, 1.f };  // #2c2c33 Separator/Border

    // Text
    inline constexpr ImVec4 Tx0{ 0.906f, 0.906f, 0.925f, 1.f };   // #e7e7ec primary text
    inline constexpr ImVec4 Tx1{ 0.643f, 0.643f, 0.686f, 1.f };   // #a4a4af dimmed
    inline constexpr ImVec4 Tx2{ 0.443f, 0.443f, 0.486f, 1.f };   // #71717c muted labels

    // Semantic colors
    inline constexpr ImVec4 Accent{ 0.690f, 0.482f, 0.941f, 1.f };  // #b07bf0 selection/acc
    inline constexpr ImVec4 Ok    { 0.275f, 0.812f, 0.545f, 1.f };  // #46cf8b good
    inline constexpr ImVec4 Warn  { 0.910f, 0.757f, 0.290f, 1.f };  // #e8c14a warning
    inline constexpr ImVec4 Hot   { 0.910f, 0.573f, 0.290f, 1.f };  // #e8924a hot/stressed
    inline constexpr ImVec4 Crit  { 0.910f, 0.376f, 0.431f, 1.f };  // #e8606e critical

    // Resource type colors (ports / wires / badges)
    inline constexpr ImVec4 RTV{ 0.910f, 0.573f, 0.290f, 1.f };   // #e8924a
    inline constexpr ImVec4 SRV{ 0.290f, 0.659f, 0.910f, 1.f };   // #4aa8e8
    inline constexpr ImVec4 UAV{ 0.275f, 0.812f, 0.545f, 1.f };   // #46cf8b
    inline constexpr ImVec4 DSV{ 0.690f, 0.482f, 0.941f, 1.f };   // #b07bf0
    inline constexpr ImVec4 CBV{ 0.910f, 0.757f, 0.290f, 1.f };   // #e8c14a

    // Legacy aliases – kept so existing code compiles unchanged
    inline constexpr ImVec4 Active  = SRV;
    inline constexpr ImVec4 Info    = SRV;
    inline constexpr ImVec4 Success = Ok;
    inline constexpr ImVec4 Warning = Warn;
    inline constexpr ImVec4 Danger  = Crit;
    inline constexpr ImVec4 Muted   = Tx2;
    inline constexpr ImVec4 Variant = Accent;
    inline constexpr ImVec4 Override= Hot;
    inline constexpr ImVec4 White   = Tx0;
    inline constexpr ImVec4 Folder  = Warn;

    inline ImU32 toU32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
    inline ImU32 toU32A(const ImVec4& c, float a) { return ImGui::ColorConvertFloat4ToU32({ c.x, c.y, c.z, a }); }
    inline ImU32 toU32Scaled(const ImVec4& c, float s) { return ImGui::ColorConvertFloat4ToU32({ c.x * s, c.y * s, c.z * s, 1.f }); }
}
