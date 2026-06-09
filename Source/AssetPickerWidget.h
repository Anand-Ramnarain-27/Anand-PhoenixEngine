#pragma once
// ---------------------------------------------------------------------------
// AssetPickerWidget — Unity-style inline asset selector for the Inspector.
//
// Usage:
//   #include "AssetPickerWidget.h"
//   if (AssetPicker::Draw("##myTex", path, AssetPicker::kTextures))
//       // path was changed
//
// The widget renders a row that looks like:
//
//   [ [T] Flames.png         v ] [x]
//
// * Left button: shows current filename (or "None") with a type tag.
//   Click → opens a searchable popup listing every matching file under Assets/.
//   Drag-drop from the Asset Browser also works on this button.
// * Right [x] button: clears the path.
//
// The popup:
//   ┌─────────────────────────────┐
//   │ 🔍 [_____________] [Refresh]│
//   ├─────────────────────────────┤
//   │  [T]  Flames.png            │
//   │  [T]  Flames 2.png          │
//   │  [T]  FireSmokeTest.png     │
//   │  ...                        │
//   └─────────────────────────────┘
//
// Paths returned are relative (e.g. "Assets/Textures/Flames.png") — the same
// format the BillboardPass / TrailPass texture loaders already expect.
// ---------------------------------------------------------------------------
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <imgui.h>
#include "AssetBrowserPanel.h"   // kDragAsset
#include "Application.h"
#include "ModuleFileSystem.h"

namespace AssetPicker {

// Preset extension filter strings (pass to Draw's extFilter parameter).
static constexpr const char* kTextures = ".png,.jpg,.jpeg,.dds,.tga,.bmp";
static constexpr const char* kMeshes   = ".gltf,.fbx,.obj";
static constexpr const char* kAll      = "";

namespace detail {

struct PickerState {
    std::string            assetsRoot;
    std::vector<std::string> files;     // relative paths
    bool                   scanned = false;
    char                   search[128] = {};
};

inline PickerState& state() {
    static PickerState s;
    return s;
}

// Check if `ext` (lowercase, WITH dot) is in a comma-separated filter string.
// Empty filter → accept everything.
inline bool extAllowed(const std::string& ext, const char* filter) {
    if (!filter || filter[0] == '\0') return true;
    // Walk comma-separated tokens
    const char* p = filter;
    while (*p) {
        const char* comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (ext.size() == len && _strnicmp(ext.c_str(), p, len) == 0)
            return true;
        if (!comma) break;
        p = comma + 1;
    }
    return false;
}

inline void scan(const char* extFilter) {
    auto& s = state();
    s.files.clear();

    s.assetsRoot = app->getFileSystem()->GetAssetsPath();
    // Strip trailing slash
    while (!s.assetsRoot.empty() &&
           (s.assetsRoot.back() == '/' || s.assetsRoot.back() == '\\'))
        s.assetsRoot.pop_back();

    namespace fs = std::filesystem;
    try {
        for (const auto& e : fs::recursive_directory_iterator(
                 s.assetsRoot,
                 fs::directory_options::skip_permission_denied)) {
            if (e.is_regular_file()) {
                std::string ext = e.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (extAllowed(ext, extFilter)) {
                    // Store as relative path using forward slashes
                    std::string rel = fs::relative(e.path(),
                                         fs::path(s.assetsRoot).parent_path())
                                         .string();
                    // Normalise to forward slashes
                    std::replace(rel.begin(), rel.end(), '\\', '/');
                    s.files.push_back(std::move(rel));
                }
            }
        }
    } catch (...) {}

    std::sort(s.files.begin(), s.files.end());
    s.scanned = true;
}

// Return just the filename portion of a path.
inline std::string filename(const std::string& path) {
    auto p = path.rfind('/');
    return (p == std::string::npos) ? path : path.substr(p + 1);
}

// Map file extension to the same coloured tag the AssetBrowser uses.
inline const char* typeTag(const std::string& path) {
    auto p = path.rfind('.');
    if (p == std::string::npos) return "[?]";
    std::string ext = path.substr(p);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".dds" || ext == ".tga" || ext == ".bmp") return "[T]";
    if (ext == ".gltf" || ext == ".fbx" || ext == ".obj") return "[M]";
    if (ext == ".json")   return "[S]";
    if (ext == ".prefab") return "[P]";
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") return "[A]";
    return "[?]";
}

inline ImVec4 typeColor(const std::string& path) {
    auto p = path.rfind('.');
    if (p == std::string::npos) return { 0.7f, 0.7f, 0.7f, 1.f };
    std::string ext = path.substr(p);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".dds" || ext == ".tga" || ext == ".bmp")
        return { 0.75f, 0.50f, 1.00f, 1.f };
    if (ext == ".gltf" || ext == ".fbx" || ext == ".obj")
        return { 0.40f, 0.80f, 1.00f, 1.f };
    return { 0.7f, 0.7f, 0.7f, 1.f };
}

} // namespace detail

// -------------------------------------------------------------------------
// Draw() — the main entry point.
//
//   uid       : unique ImGui ID string (e.g. "##myTex")
//   path      : current path (modified in-place on selection)
//   extFilter : comma-separated lowercase extensions to show (e.g. ".png,.dds")
//               Pass kTextures, kMeshes, or kAll for convenience.
//
// Returns true if `path` was changed this frame.
// -------------------------------------------------------------------------
inline bool Draw(const char* uid,
                 std::string& path,
                 const char* extFilter = kTextures) {
    bool changed = false;
    auto& st = detail::state();

    // ---- Build the popup id from the widget uid -------------------------
    std::string popupId = std::string("AssetPickerPopup_") + uid;

    // ---- Main button: shows type tag + truncated filename ---------------
    const std::string fname = path.empty() ? "None" : detail::filename(path);
    const std::string btnLabel = (path.empty() ? "[?] " : std::string(detail::typeTag(path)) + " ") + fname;

    // Button fills available width minus a small "x" clear button on the right.
    const float clearW = 24.f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float btnW = ImGui::GetContentRegionAvail().x - clearW - spacing;

    // ImGui uses everything after "##" as the ID, so label visible text is
    // just the type tag + filename; the uid becomes the ID part.
    std::string btnImguiLabel = btnLabel + uid;

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.18f, 0.22f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.35f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.35f, 0.35f, 0.45f, 1.f));
    bool openPopup = ImGui::Button(btnImguiLabel.c_str(), ImVec2(btnW, 0));
    ImGui::PopStyleColor(3);

    // Drag-drop target on the main button — same as the old InputText.
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload(kDragAsset)) {
            path.assign(static_cast<const char*>(pl->Data), pl->DataSize - 1);
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }

    if (!path.empty()) {
        // Tooltip with full path on hover.
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", path.c_str());
    }

    // ---- Clear button ---------------------------------------------------
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.45f, 0.15f, 0.15f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.20f, 0.20f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.80f, 0.25f, 0.25f, 1.f));
    if (ImGui::Button(("x" + std::string(uid) + "_clear").c_str(), ImVec2(clearW, 0))) {
        path.clear();
        changed = true;
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear");

    // ---- Open picker popup ----------------------------------------------
    if (openPopup) {
        // Re-scan on every open so newly imported files appear immediately.
        detail::scan(extFilter);
        memset(st.search, 0, sizeof(st.search));
        ImGui::OpenPopup(popupId.c_str());
    }

    // ---- Popup ----------------------------------------------------------
    ImGui::SetNextWindowSize(ImVec2(380.f, 340.f), ImGuiCond_Always);
    if (ImGui::BeginPopup(popupId.c_str())) {
        // Search bar row
        ImGui::PushItemWidth(-65.f);
        ImGui::InputText("##assetSearch", st.search, sizeof(st.search));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh")) detail::scan(extFilter);
        ImGui::Separator();

        // "None" option at the top
        bool selNone = path.empty();
        if (ImGui::Selectable("[?] None", selNone)) {
            path.clear();
            changed = true;
            ImGui::CloseCurrentPopup();
        }

        // File list
        const std::string filterStr(st.search);
        ImGui::BeginChild("##assetList", ImVec2(0, 260.f), false);

        for (const auto& f : st.files) {
            const std::string fname2 = detail::filename(f);
            // Filter: search string must appear in the filename (case-insensitive)
            if (!filterStr.empty()) {
                std::string lower = fname2;
                std::string lowerFilter = filterStr;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
                if (lower.find(lowerFilter) == std::string::npos) continue;
            }

            // Row: coloured type tag + filename
            ImGui::PushStyleColor(ImGuiCol_Text, detail::typeColor(f));
            ImGui::TextUnformatted(detail::typeTag(f));
            ImGui::PopStyleColor();

            ImGui::SameLine(38.f);
            bool sel = (f == path);
            if (ImGui::Selectable(("##ap_" + f).c_str(), sel,
                                   ImGuiSelectableFlags_SpanAllColumns,
                                   ImVec2(0, 0))) {
                path = f;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine(38.f);
            ImGui::TextUnformatted(fname2.c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", f.c_str());
        }

        ImGui::EndChild();
        ImGui::EndPopup();
    }

    return changed;
}

} // namespace AssetPicker
