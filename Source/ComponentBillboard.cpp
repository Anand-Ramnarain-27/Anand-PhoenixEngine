#include "Globals.h"
#include "ComponentBillboard.h"
#include "GameObject.h"
#include "AssetBrowserPanel.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "TextureImporter.h"
#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

ComponentBillboard::ComponentBillboard(GameObject* owner) : Component(owner) {}

namespace {
    // Recursively collects image asset paths under <project>/Assets so the
    // inspector can offer a "pick from project" dropdown alongside drag-and-drop.
    std::vector<std::string> collectTextureAssets() {
        std::vector<std::string> out;
        std::string lib = app->getFileSystem()->GetLibraryPath();
        while (!lib.empty() && (lib.back() == '/' || lib.back() == '\\')) lib.pop_back();
        std::filesystem::path assetsDir = std::filesystem::path(lib).parent_path() / "Assets";

        std::error_code ec;
        if (!std::filesystem::exists(assetsDir, ec)) return out;

        for (auto it = std::filesystem::recursive_directory_iterator(assetsDir, ec);
             it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            if (it->is_directory(ec)) continue;

            std::string ext = it->path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds")
                out.push_back(it->path().string());
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    // Cached GPU thumbnail for the asset-picker grid (mirrors AssetBrowserPanel's
    // thumbnail cache so the picker shows real previews like Unity's object picker).
    struct PickerThumb {
        ComPtr<ID3D12Resource> tex;
        D3D12_GPU_DESCRIPTOR_HANDLE srv = {};
        bool attempted = false;
    };

    constexpr float kPickerThumbSize = 64.0f;

    // Draws Unity-style "Select Texture" object-picker popup: search bar + thumbnail
    // grid (with a leading "None" tile). Returns true and writes `outPath` when the
    // user picks an entry (outPath is cleared for "None"); the popup closes itself.
    bool drawTexturePickerPopup(const char* popupId, const std::vector<std::string>& assets,
                                std::string& outPath) {
        bool changed = false;
        ImGui::SetNextWindowSize(ImVec2(360, 420), ImGuiCond_FirstUseEver);
        if (!ImGui::BeginPopup(popupId)) return false;

        static char s_search[128] = {};
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##billboardPickerSearch", "Search...", s_search, sizeof(s_search));
        std::string filter = s_search;
        std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

        ImGui::Separator();
        ImGui::BeginChild("##billboardPickerGrid", ImVec2(0, 0), false);

        static std::unordered_map<std::string, PickerThumb> s_thumbs;

        const float cellW = kPickerThumbSize + 16.0f;
        const float cellH = kPickerThumbSize + 28.0f;
        const float avail = ImGui::GetContentRegionAvail().x;
        const int cols = std::max(1, (int)(avail / cellW));

        int col = 0;

        // "None" entry — clears the texture, exactly like Unity's object picker.
        {
            ImGui::BeginGroup();
            bool clicked = ImGui::Selectable("##pickNone", false, ImGuiSelectableFlags_AllowDoubleClick,
                                             ImVec2(kPickerThumbSize, kPickerThumbSize + 18.0f));
            ImVec2 cellMin = ImGui::GetItemRectMin();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(cellMin, ImVec2(cellMin.x + kPickerThumbSize, cellMin.y + kPickerThumbSize),
                              IM_COL32(60, 60, 60, 255), 4.0f);
            dl->AddText(ImVec2(cellMin.x + kPickerThumbSize * 0.5f - 12, cellMin.y + kPickerThumbSize * 0.5f - 7),
                        IM_COL32(200, 200, 200, 255), "None");
            ImGui::SetCursorScreenPos(ImVec2(cellMin.x, cellMin.y + kPickerThumbSize + 2.0f));
            ImGui::TextUnformatted("None");
            ImGui::EndGroup();
            if (clicked) { outPath.clear(); changed = true; ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            ++col;
        }

        for (const auto& path : assets) {
            std::string name = std::filesystem::path(path).filename().string();
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (!filter.empty() && lower.find(filter) == std::string::npos) continue;

            ImGui::PushID(path.c_str());
            ImGui::BeginGroup();
            bool clicked = ImGui::Selectable("##pickItem", false, ImGuiSelectableFlags_AllowDoubleClick,
                                             ImVec2(kPickerThumbSize, kPickerThumbSize + 18.0f));
            ImVec2 cellMin = ImGui::GetItemRectMin();

            auto& th = s_thumbs[path];
            if (!th.attempted) { th.attempted = true; TextureImporter::Load(path, th.tex, th.srv); }
            if (th.tex) {
                ImGui::GetWindowDrawList()->AddImage((ImTextureID)th.srv.ptr, cellMin,
                    ImVec2(cellMin.x + kPickerThumbSize, cellMin.y + kPickerThumbSize));
            } else {
                ImGui::GetWindowDrawList()->AddRectFilled(cellMin,
                    ImVec2(cellMin.x + kPickerThumbSize, cellMin.y + kPickerThumbSize), IM_COL32(80, 80, 80, 255), 4.0f);
            }

            std::string disp = name;
            if (disp.size() > 12) disp = disp.substr(0, 10) + "..";
            float tw = ImGui::CalcTextSize(disp.c_str()).x;
            ImGui::SetCursorScreenPos(ImVec2(cellMin.x + (kPickerThumbSize - tw) * 0.5f, cellMin.y + kPickerThumbSize + 2.0f));
            ImGui::TextUnformatted(disp.c_str());
            ImGui::EndGroup();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path.c_str());
            if (clicked) { outPath = path; changed = true; ImGui::CloseCurrentPopup(); }
            ImGui::PopID();

            ++col;
            if (col < cols) ImGui::SameLine();
            else col = 0;
        }

        ImGui::EndChild();
        if (changed) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return changed;
    }
}

void ComponentBillboard::update(float dt) {
    if (!enabled || framesPerSecond <= 0.f) return;

    const int totalTiles = std::max(1, sheetColumns * sheetRows);
    m_currentFrame += framesPerSecond * dt;

    if (loop) {
        m_currentFrame = fmodf(m_currentFrame, (float)totalTiles);
        if (m_currentFrame < 0.f) m_currentFrame += (float)totalTiles;
    } else {
        m_currentFrame = std::min(m_currentFrame, (float)(totalTiles - 1));
    }
}

void ComponentBillboard::onEditor() {
    ImGui::Checkbox("Enabled##billboard", &enabled);

    // Unity-style object field: a thumbnail swatch + name that opens an asset
    // picker popup when clicked, plus drag-and-drop support straight onto the field.
    {
        static std::vector<std::string> s_textureAssets;
        static bool s_scanned = false;
        if (!s_scanned) { s_textureAssets = collectTextureAssets(); s_scanned = true; }

        static std::unordered_map<std::string, PickerThumb> s_fieldThumbs;

        ImGui::TextUnformatted("Texture");
        ImGui::SameLine(90.0f);

        const float fieldH = 36.0f;
        ImVec2 fieldSize(ImGui::GetContentRegionAvail().x - 56.0f, fieldH);
        ImGui::BeginGroup();
        bool fieldClicked = ImGui::Selectable("##billboardTexField", false, ImGuiSelectableFlags_AllowDoubleClick, fieldSize);
        ImVec2 fMin = ImGui::GetItemRectMin();
        ImVec2 fMax = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRect(fMin, fMax, IM_COL32(90, 90, 90, 255), 3.0f);

        if (!texturePath.empty()) {
            auto& th = s_fieldThumbs[texturePath];
            if (!th.attempted) { th.attempted = true; TextureImporter::Load(texturePath, th.tex, th.srv); }
            const float thumb = fieldH - 6.0f;
            ImVec2 thumbMin(fMin.x + 3.0f, fMin.y + 3.0f);
            if (th.tex)
                dl->AddImage((ImTextureID)th.srv.ptr, thumbMin, ImVec2(thumbMin.x + thumb, thumbMin.y + thumb));
            else
                dl->AddRectFilled(thumbMin, ImVec2(thumbMin.x + thumb, thumbMin.y + thumb), IM_COL32(80, 80, 80, 255), 3.0f);

            std::string name = std::filesystem::path(texturePath).filename().string();
            dl->AddText(ImVec2(thumbMin.x + thumb + 8.0f, fMin.y + (fieldH - ImGui::GetTextLineHeight()) * 0.5f),
                        IM_COL32(220, 220, 220, 255), name.c_str());
        } else {
            dl->AddText(ImVec2(fMin.x + 8.0f, fMin.y + (fieldH - ImGui::GetTextLineHeight()) * 0.5f),
                        IM_COL32(140, 140, 140, 255), "None (Texture)");
        }
        ImGui::EndGroup();

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click to pick a texture, or drag one here from the Asset Browser.\n%s",
                              texturePath.empty() ? "" : texturePath.c_str());

        // Drag-and-drop target straight onto the object field (Unity behaviour)
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload(kDragAsset)) {
                std::string dropped(static_cast<const char*>(pl->Data), pl->DataSize - 1);
                std::string lower = dropped;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                auto endsWith = [&](const char* ext) {
                    size_t n = strlen(ext);
                    return lower.size() >= n && lower.compare(lower.size() - n, n, ext) == 0;
                };
                if (endsWith(".png") || endsWith(".jpg") || endsWith(".jpeg") || endsWith(".dds"))
                    texturePath = dropped;
            }
            ImGui::EndDragDropTarget();
        }

        if (fieldClicked) {
            s_textureAssets = collectTextureAssets(); // refresh on open, like Unity rescans on picker open
            ImGui::OpenPopup("##billboardTexPicker");
        }
        drawTexturePickerPopup("##billboardTexPicker", s_textureAssets, texturePath);

        // Small circular-looking "select" button on the right, mirroring Unity's
        // object-field picker button (the little circle at the field's right edge).
        ImGui::SameLine();
        if (ImGui::Button("o##billboardTexPickBtn", ImVec2(20, fieldH))) {
            s_textureAssets = collectTextureAssets();
            ImGui::OpenPopup("##billboardTexPicker");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pick texture asset...");

        if (!texturePath.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("X##clearBillboardTex"))
                texturePath.clear();
        }
    }

    static const char* kAlignments[] = { "Screen", "World (look-at)", "Axial" };
    int alignIdx = (int)alignment;
    if (ImGui::Combo("Alignment", &alignIdx, kAlignments, IM_ARRAYSIZE(kAlignments)))
        alignment = (Alignment)alignIdx;

    ImGui::DragFloat2("Size", &size.x, 0.05f, 0.01f, 1000.f);
    ImGui::ColorEdit4("Tint", &tint.x);

    ImGui::Separator();
    ImGui::TextUnformatted("Sprite sheet animation");
    ImGui::DragInt("Columns", &sheetColumns, 1.f, 1, 64);
    ImGui::DragInt("Rows", &sheetRows, 1.f, 1, 64);
    ImGui::DragFloat("Frames / second", &framesPerSecond, 0.1f, 0.f, 240.f);
    ImGui::Checkbox("Loop", &loop);
}

void ComponentBillboard::onSave(std::string& outJson) const {
    outJson += "\"texturePath\":\"" + texturePath + "\",";
    outJson += "\"alignment\":" + std::to_string((int)alignment) + ",";
    outJson += "\"size\":[" + std::to_string(size.x) + "," + std::to_string(size.y) + "],";
    outJson += "\"tint\":[" + std::to_string(tint.x) + "," + std::to_string(tint.y) + "," +
                               std::to_string(tint.z) + "," + std::to_string(tint.w) + "],";
    outJson += "\"sheetColumns\":" + std::to_string(sheetColumns) + ",";
    outJson += "\"sheetRows\":" + std::to_string(sheetRows) + ",";
    outJson += "\"framesPerSecond\":" + std::to_string(framesPerSecond) + ",";
    outJson += "\"loop\":" + std::string(loop ? "true" : "false") + ",";
    outJson += "\"enabled\":" + std::string(enabled ? "true" : "false");
}

void ComponentBillboard::onLoad(const std::string& json) {
    auto extract = [&](const char* key) -> std::string {
        std::string k = "\"" + std::string(key) + "\":";
        auto pos = json.find(k);
        if (pos == std::string::npos) return {};
        pos += k.size();
        if (json[pos] == '"') {
            ++pos;
            auto end = json.find('"', pos);
            return json.substr(pos, end - pos);
        }
        if (json[pos] == '[') {
            auto end = json.find(']', pos);
            return json.substr(pos, end - pos + 1);
        }
        auto end = json.find_first_of(",}", pos);
        return json.substr(pos, end - pos);
    };

    auto extractArray = [&](const std::string& arr, float* out, int count) {
        if (arr.size() < 2) return;
        std::string inner = arr.substr(1, arr.size() - 2);
        size_t start = 0;
        for (int i = 0; i < count; ++i) {
            size_t comma = inner.find(',', start);
            std::string token = (comma == std::string::npos) ? inner.substr(start)
                                                              : inner.substr(start, comma - start);
            if (!token.empty()) out[i] = std::stof(token);
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
    };

    texturePath = extract("texturePath");

    auto align = extract("alignment");
    if (!align.empty()) alignment = (Alignment)std::stoi(align);

    auto sz = extract("size");
    if (!sz.empty()) extractArray(sz, &size.x, 2);

    auto tn = extract("tint");
    if (!tn.empty()) extractArray(tn, &tint.x, 4);

    auto cols = extract("sheetColumns");
    if (!cols.empty()) sheetColumns = std::stoi(cols);

    auto rows = extract("sheetRows");
    if (!rows.empty()) sheetRows = std::stoi(rows);

    auto fps = extract("framesPerSecond");
    if (!fps.empty()) framesPerSecond = std::stof(fps);

    auto lp = extract("loop");
    if (!lp.empty()) loop = (lp == "true");

    auto en = extract("enabled");
    if (!en.empty()) enabled = (en == "true");
}
