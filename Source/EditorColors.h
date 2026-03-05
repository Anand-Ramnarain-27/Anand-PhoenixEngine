#pragma once
#include <imgui.h>

namespace EditorColors {
    inline constexpr ImVec4 Active{ 0.26f, 0.59f, 0.98f, 1.00f };
    inline constexpr ImVec4 Info{ 0.26f, 0.80f, 0.98f, 1.00f };
    inline constexpr ImVec4 Success{ 0.40f, 0.85f, 0.40f, 1.00f };
    inline constexpr ImVec4 Warning{ 0.95f, 0.75f, 0.20f, 1.00f };
    inline constexpr ImVec4 Danger{ 0.90f, 0.30f, 0.30f, 1.00f };
    inline constexpr ImVec4 Muted{ 0.50f, 0.50f, 0.50f, 1.00f };
    inline constexpr ImVec4 Variant{ 0.70f, 0.50f, 0.95f, 1.00f };
    inline constexpr ImVec4 Override{ 0.95f, 0.65f, 0.15f, 1.00f };
    inline constexpr ImVec4 White{ 1.00f, 1.00f, 1.00f, 1.00f };
    inline constexpr ImVec4 Folder{ 0.95f, 0.78f, 0.35f, 1.00f };

    inline ImU32 toU32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
    inline ImU32 toU32Scaled(const ImVec4& c, float s) { return ImGui::ColorConvertFloat4ToU32({ c.x * s, c.y * s, c.z * s, 1.f }); }
}