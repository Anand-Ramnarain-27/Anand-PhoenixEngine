#pragma once
#include <imgui.h>

namespace EditorColors {
    // ---- Legacy semantic colors (kept for backward compat) ----
    inline constexpr ImVec4 Active { 0.26f, 0.59f, 0.98f, 1.00f };
    inline constexpr ImVec4 Info { 0.26f, 0.80f, 0.98f, 1.00f };
    inline constexpr ImVec4 Success { 0.40f, 0.85f, 0.40f, 1.00f };
    inline constexpr ImVec4 Warning { 0.95f, 0.75f, 0.20f, 1.00f };
    inline constexpr ImVec4 Danger { 0.90f, 0.30f, 0.30f, 1.00f };
    inline constexpr ImVec4 Muted { 0.50f, 0.50f, 0.50f, 1.00f };
    inline constexpr ImVec4 Variant { 0.70f, 0.50f, 0.95f, 1.00f };
    inline constexpr ImVec4 Override{ 0.95f, 0.65f, 0.15f, 1.00f };
    inline constexpr ImVec4 White { 1.00f, 1.00f, 1.00f, 1.00f };
    inline constexpr ImVec4 Folder { 0.95f, 0.78f, 0.35f, 1.00f };

    // ---- Phoenix design-system colors ----
    // Surfaces
    inline constexpr ImVec4 BgVoid { 0.043f, 0.043f, 0.051f, 1.f }; // #0b0b0d
    inline constexpr ImVec4 Bg0 { 0.078f, 0.078f, 0.090f, 1.f }; // #141417
    inline constexpr ImVec4 Bg1 { 0.102f, 0.102f, 0.118f, 1.f }; // #1a1a1e
    inline constexpr ImVec4 Bg2 { 0.129f, 0.129f, 0.149f, 1.f }; // #212126
    inline constexpr ImVec4 Bg3 { 0.165f, 0.165f, 0.192f, 1.f }; // #2a2a31
    inline constexpr ImVec4 Bg4 { 0.204f, 0.204f, 0.239f, 1.f }; // #34343d
    inline constexpr ImVec4 Line { 0.173f, 0.173f, 0.200f, 1.f }; // #2c2c33
    inline constexpr ImVec4 Line2 { 0.227f, 0.227f, 0.267f, 1.f }; // #3a3a44

    // Text
    inline constexpr ImVec4 Tx0 { 0.906f, 0.906f, 0.925f, 1.f }; // #e7e7ec
    inline constexpr ImVec4 Tx1 { 0.643f, 0.643f, 0.686f, 1.f }; // #a4a4af
    inline constexpr ImVec4 Tx2 { 0.443f, 0.443f, 0.486f, 1.f }; // #71717c

    // Accent
    inline constexpr ImVec4 Acc { 0.690f, 0.482f, 0.941f, 1.f }; // #b07bf0
    inline constexpr ImVec4 Acc2 { 0.780f, 0.608f, 1.000f, 1.f }; // #c79bff
    inline constexpr ImVec4 AccDim { 0.690f, 0.482f, 0.941f, 0.16f };

    // Status
    inline constexpr ImVec4 Ok { 0.275f, 0.812f, 0.545f, 1.f }; // #46cf8b
    inline constexpr ImVec4 Warn { 0.910f, 0.757f, 0.290f, 1.f }; // #e8c14a
    inline constexpr ImVec4 Hot { 0.910f, 0.573f, 0.290f, 1.f }; // #e8924a
    inline constexpr ImVec4 Crit { 0.910f, 0.376f, 0.431f, 1.f }; // #e8606e
    inline constexpr ImVec4 Inf { 0.290f, 0.659f, 0.910f, 1.f }; // #4aa8e8

    // Resource type colors
    inline constexpr ImVec4 ResRTV { 0.910f, 0.573f, 0.290f, 1.f }; // #e8924a
    inline constexpr ImVec4 ResSRV { 0.290f, 0.659f, 0.910f, 1.f }; // #4aa8e8
    inline constexpr ImVec4 ResUAV { 0.275f, 0.812f, 0.545f, 1.f }; // #46cf8b
    inline constexpr ImVec4 ResDSV { 0.690f, 0.482f, 0.941f, 1.f }; // #b07bf0
    inline constexpr ImVec4 ResCBV { 0.910f, 0.757f, 0.290f, 1.f }; // #e8c14a

    inline ImU32 toU32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
    inline ImU32 toU32Scaled(const ImVec4& c, float s) { return ImGui::ColorConvertFloat4ToU32({ c.x*s, c.y*s, c.z*s, 1.f }); }

    // Returns ok/warn/hot/crit color based on a ms value
    inline ImVec4 msColor(float ms){
        if (ms < 0.5f) return Ok;
        if (ms < 1.5f) return Warn;
        if (ms < 2.6f) return Hot;
        return Crit;
    }
}

// PhoenixTheme_Apply() is implemented in ImGuiPass.cpp (static, no header needed).
