#include "Globals.h"
#include "AssetBrowserPanel.h"
#include "ModuleEditor.h"
#include "ModuleAssets.h"
#include "Application.h"
#include "ModuleScene.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "ResourceMaterial.h"
#include "ComponentMesh.h"
#include "ComponentAnimation.h"
#include "ResourceAnimation.h"
#include "StateMachineResource.h"
#include "ModuleResources.h"
#include "PrefabManager.h"
#include "TextureImporter.h"
#include "PrimitiveFactory.h"
#include "EditorSelection.h"
#include "EditorColors.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <filesystem>
#include <algorithm>

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

static const char* kSkipDirs[] = { "build","Build","x64","x86","Debug","Release",".vs",".git","CMakeFiles","obj","bin","Intermediate","Saved","DerivedDataCache" };

static bool isDirSkipped(const std::string& name) {
    for (auto* s : kSkipDirs) if (name == s) return true;
    return false;
}

bool AssetBrowserPanel::isAssetFile(const std::string& ext) const {
    static const char* kAllowed[] = { ".gltf",".fbx",".obj",".dds",".png",".jpg",".jpeg",".json",".prefab",".wav",".mp3",".ogg",".anim",".sm" };
    for (auto* e : kAllowed) if (ext == e) return true;
    return false;
}

ImVec4 AssetBrowserPanel::typeColor(const std::string& ext) const {
    if (ext == ".gltf" || ext == ".fbx" || ext == ".obj") return { 0.40f, 0.80f, 1.00f, 1.f };
    if (ext == ".dds" || ext == ".png" || ext == ".jpg" || ext == ".jpeg") return { 0.75f, 0.50f, 1.00f, 1.f };
    if (ext == ".json") return { 1.00f, 0.80f, 0.35f, 1.f };
    if (ext == ".prefab") return { 0.35f, 0.90f, 0.45f, 1.f };
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") return { 1.00f, 0.55f, 0.35f, 1.f };
    if (ext == ".anim") return { 1.00f, 0.60f, 0.90f, 1.f };  // pink/magenta
    if (ext == ".sm")   return { 0.60f, 0.90f, 1.00f, 1.f };  // light cyan
    return EditorColors::Muted;
}

const char* AssetBrowserPanel::typeIcon(const std::string& ext) const {
    if (ext == ".gltf" || ext == ".fbx" || ext == ".obj") return "[M]";
    if (ext == ".dds" || ext == ".png" || ext == ".jpg" || ext == ".jpeg") return "[T]";
    if (ext == ".json") return "[S]";
    if (ext == ".prefab") return "[P]";
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") return "[A]";
    if (ext == ".anim") return "[AN]";
    if (ext == ".sm")   return "[SM]";
    return "[?]";
}

void AssetBrowserPanel::navigateTo(const std::string& path) {
    m_currentPath = path; m_selectedPath.clear(); m_dirty = true;
}

void AssetBrowserPanel::refreshCurrentDir() {
    m_entries.clear(); m_dirty = false;
    if (m_currentPath.empty()) return;
    std::vector<Entry> dirs, files;
    try {
        for (const auto& e : fs::directory_iterator(m_currentPath)) {
            Entry en;
            en.path = e.path().string();
            en.name = e.path().filename().string();
            en.isDir = e.is_directory();
            if (en.isDir) { dirs.push_back(std::move(en)); }
            else {
                en.ext = e.path().extension().string();
                std::transform(en.ext.begin(), en.ext.end(), en.ext.begin(), ::tolower);
                if (isAssetFile(en.ext)) files.push_back(std::move(en));
            }
        }
    }
    catch (...) {}
    auto byName = [](const Entry& a, const Entry& b) { return a.name < b.name; };
    std::sort(dirs.begin(), dirs.end(), byName);
    std::sort(files.begin(), files.end(), byName);
    m_entries.insert(m_entries.end(), dirs.begin(), dirs.end());
    m_entries.insert(m_entries.end(), files.begin(), files.end());
}

void AssetBrowserPanel::drawContent() {
    if (m_roots.empty()) {
        std::string lib = app->getFileSystem()->GetLibraryPath();
        while (!lib.empty() && (lib.back() == '/' || lib.back() == '\\')) lib.pop_back();
        fs::path projectRoot = fs::path(lib).parent_path();
        for (auto& [rel, label] : std::vector<std::pair<std::string, std::string>>{
                {"Assets","Assets"},
                {"Library","Library"},
                {"Library/Animations","Animations"} }) {
            std::string full = (projectRoot / rel).string();
            if (fs::exists(full)) m_roots.push_back({ full, label });
        }
        if (!m_roots.empty()) navigateTo(m_roots[0].path);
    }
    if (m_dirty) refreshCurrentDir();

    EditorSelection& sel = m_editor->getSelection();
    bool isInst = sel.has() && PrefabManager::isPrefabInstance(sel.object);
    float instanceBarH = isInst ? 52.0f : 0.0f;
    float mainH = ImGui::GetContentRegionAvail().y - kStatusH - instanceBarH - (isInst ? 4.0f : 0.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.f));
    ImGui::BeginChild("##ABTree", ImVec2(kTreeW, mainH), false);
    drawFolderTree();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 2);
    ImGui::BeginChild("##ABRight", ImVec2(0, mainH), false);
    drawBreadcrumb();
    ImGui::Separator();
    drawThumbnailGrid();
    ImGui::EndChild();
    if (isInst) { ImGui::Separator(); drawPrefabInstanceBar(); }
    ImGui::Separator();
    drawStatusBar();
    drawModals();
}

void AssetBrowserPanel::drawFolderTree() {
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);
    textMuted("FOLDERS");
    ImGui::Separator();
    for (const auto& root : m_roots) drawFolderNode(root.path, true);
}

void AssetBrowserPanel::drawFolderNode(const std::string& absPath, bool forceOpen) {
    std::vector<std::string> subDirs;
    try {
        for (const auto& e : fs::directory_iterator(absPath)) {
            if (!e.is_directory()) continue;
            std::string dname = e.path().filename().string();
            if (dname.empty() || dname[0] == '.' || isDirSkipped(dname)) continue;
            subDirs.push_back(e.path().string());
        }
    }
    catch (...) {}
    std::sort(subDirs.begin(), subDirs.end());

    std::string name = fs::path(absPath).filename().string();
    for (const auto& r : m_roots) if (r.path == absPath) { name = r.label; break; }

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
        (subDirs.empty() ? ImGuiTreeNodeFlags_Leaf : 0) |
        (absPath == m_currentPath ? ImGuiTreeNodeFlags_Selected : 0) |
        (forceOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);

    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Folder);
    bool open = ImGui::TreeNodeEx(absPath.c_str(), flags, "%s", name.c_str());
    ImGui::PopStyleColor();
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) navigateTo(absPath);
    if (open) {
        for (const auto& sub : subDirs) drawFolderNode(sub, false);
        ImGui::TreePop();
    }
}

void AssetBrowserPanel::drawBreadcrumb() {
    const float searchW = 150.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - searchW);
    ImGui::SetNextItemWidth(searchW);
    ImGui::InputTextWithHint("##abSearch", "Search...", m_searchBuf, sizeof(m_searchBuf));
    ImGui::SetCursorPos({ 4, ImGui::GetCursorPosY() - ImGui::GetFrameHeight() - 2 });

    std::string rootPath, rootLabel;
    for (const auto& r : m_roots)
        if (m_currentPath.rfind(r.path, 0) == 0) { rootPath = r.path; rootLabel = r.label; break; }

    if (!rootPath.empty()) {
        fs::path rel = fs::path(m_currentPath).lexically_relative(fs::path(rootPath).parent_path());
        fs::path built = fs::path(rootPath).parent_path();
        bool first = true;
        std::vector<std::pair<std::string, std::string>> crumbs;
        for (const auto& seg : rel) {
            built /= seg;
            crumbs.push_back({ first ? rootLabel : seg.string(), built.string() });
            first = false;
        }
        for (int i = 0; i < (int)crumbs.size(); ++i) {
            if (i > 0) { ImGui::SameLine(0, 2); textMuted(">"); ImGui::SameLine(0, 2); }
            bool last = (i == (int)crumbs.size() - 1);
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Text, last ? EditorColors::White : EditorColors::Muted);
            if (ImGui::SmallButton(crumbs[i].first.c_str())) navigateTo(crumbs[i].second);
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }
    ImGui::Spacing();
}

void AssetBrowserPanel::drawThumbnailGrid() {
    std::string filterLow = toLower(m_searchBuf);
    const float avail = ImGui::GetContentRegionAvail().x;
    const float cellW = kThumbSize + 24.0f;
    const float cellH = kThumbSize + 30.0f;
    const float pad = 8.0f;
    const int cols = std::max(1, (int)((avail - pad) / (cellW + pad)));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.11f, 0.11f, 1.f));
    ImGui::BeginChild("##ABGrid", ImVec2(0, 0), false);

    std::vector<int> visible;
    for (int i = 0; i < (int)m_entries.size(); ++i) {
        if (!filterLow.empty() && toLower(m_entries[i].name).find(filterLow) == std::string::npos) continue;
        visible.push_back(i);
    }

    if (visible.empty()) {
        ImGui::SetCursorPos({ 12, 12 });
        textMuted("No assets here.");
        drawEmptyAreaContextMenu();
    }
    else {
        int rows = ((int)visible.size() + cols - 1) / cols;
        float totalH = rows * (cellH + pad) + pad;
        ImGui::Dummy(ImVec2(avail, totalH));

        for (int vi = 0; vi < (int)visible.size(); ++vi) {
            int idx = visible[vi];
            const Entry& e = m_entries[idx];
            int c = vi % cols, r = vi / cols;
            float cx = pad + c * (cellW + pad);
            float cy = pad + r * (cellH + pad);

            ImGui::SetCursorPos({ cx, cy });
            bool selected = (m_selectedPath == e.path);
            ImGui::PushID(idx);
            ImGui::PushStyleColor(ImGuiCol_Header, selected ? ImVec4(0.26f, 0.59f, 0.98f, 0.55f) : ImVec4(0.26f, 0.59f, 0.98f, 0.00f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.25f));
            bool clicked = ImGui::Selectable("##cell", selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(cellW, cellH));
            ImGui::PopStyleColor(2);

            if (clicked) {
                m_selectedPath = e.path;
                if (ImGui::IsMouseDoubleClicked(0)) {
                    if (e.isDir) navigateTo(e.path);
                    else if (e.ext == ".prefab") m_editor->enterPrefabEdit(fs::path(e.name).stem().string());
                    else spawnAsset(e.path);
                }
            }

            if (!e.isDir && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                ImGui::SetDragDropPayload(kDragAsset, e.path.c_str(), e.path.size() + 1);
                ImVec4 c4 = typeColor(e.ext);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(c4.x * 0.15f, c4.y * 0.15f, c4.z * 0.15f, 1.f));
                ImGui::BeginChild("##dp", ImVec2(120, 28), true);
                ImGui::PushStyleColor(ImGuiCol_Text, c4);
                ImGui::Text(" %s", typeIcon(e.ext));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                std::string dn = fs::path(e.name).stem().string();
                if (dn.size() > 12) dn = dn.substr(0, 10) + "..";
                ImGui::TextUnformatted(dn.c_str());
                ImGui::EndChild();
                ImGui::PopStyleColor();
                ImGui::EndDragDropSource();
            }

            ImVec2 thumbPos = { cx + (cellW - kThumbSize) * 0.5f, cy + 4.0f };
            if (e.isDir) {
                drawColoredBox(thumbPos, EditorColors::Folder, nullptr);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 wp = ImGui::GetWindowPos();
                float sx = wp.x + thumbPos.x - ImGui::GetScrollX();
                float sy = wp.y + thumbPos.y - ImGui::GetScrollY();
                dl->AddText({ sx + kThumbSize * 0.5f - 9, sy + kThumbSize * 0.5f - 7 }, IM_COL32(255, 215, 60, 255), "[ ]");
            }
            else if (e.ext == ".dds" || e.ext == ".png" || e.ext == ".jpg" || e.ext == ".jpeg") {
                auto& th = m_thumbCache[e.path];
                if (!th.attempted) { th.attempted = true; TextureImporter::Load(e.path, th.tex, th.srv); }
                ImGui::SetCursorPos(thumbPos);
                if (th.tex) ImGui::Image((ImTextureID)th.srv.ptr, ImVec2(kThumbSize, kThumbSize));
                else drawColoredBox(thumbPos, typeColor(e.ext), typeIcon(e.ext));
            }
            else drawColoredBox(thumbPos, typeColor(e.ext), typeIcon(e.ext));

            std::string disp = e.isDir ? e.name : fs::path(e.name).stem().string();
            if (disp.size() > 10) disp = disp.substr(0, 8) + "..";
            float tw = ImGui::CalcTextSize(disp.c_str()).x;
            ImGui::SetCursorPos({ cx + (cellW - tw) * 0.5f, cy + kThumbSize + 8.0f });
            ImGui::PushStyleColor(ImGuiCol_Text, selected ? EditorColors::White : ImVec4(0.82f, 0.82f, 0.82f, 1.f));
            ImGui::TextUnformatted(disp.c_str());
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e.name.c_str());
            drawItemContextMenu(idx);
            ImGui::PopID();
        }
        drawEmptyAreaContextMenu();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AssetBrowserPanel::drawItemContextMenu(int idx) {
    if (!ImGui::BeginPopupContextItem("##abctx")) return;
    const Entry& e = m_entries[idx];
    ImGui::PushStyleColor(ImGuiCol_Text, e.isDir ? EditorColors::Folder : typeColor(e.ext));
    ImGui::Text(" %s  %s", e.isDir ? "[F]" : typeIcon(e.ext), e.name.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (e.isDir) {
        if (ImGui::MenuItem("Open Folder")) navigateTo(e.path);
    }
    else {
        if (ImGui::MenuItem("Add to Scene")) spawnAsset(e.path);
        if (ImGui::MenuItem("Show in Explorer")) ShellExecuteA(nullptr, "explore", fs::path(e.path).parent_path().string().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
        ImGui::Separator();
        if (e.ext == ".prefab") {
            std::string pfName = fs::path(e.name).stem().string();
            if (ImGui::MenuItem("Edit Prefab...")) m_editor->enterPrefabEdit(pfName);
            ImGui::Separator();
            if (ImGui::MenuItem("Instantiate Prefab")) prefabInstantiate(pfName);
            if (ImGui::MenuItem("Create Variant...")) { m_showVariantModal = true; strncpy_s(m_variantSrcBuf, pfName.c_str(), sizeof(m_variantSrcBuf) - 1); snprintf(m_variantDstBuf, sizeof(m_variantDstBuf), "%s_variant", pfName.c_str()); }
            if (ImGui::MenuItem("Rename...")) { m_renamingPrefab = true; strncpy_s(m_renameSrcBuf, pfName.c_str(), sizeof(m_renameSrcBuf) - 1); strncpy_s(m_renameDstBuf, pfName.c_str(), sizeof(m_renameDstBuf) - 1); }
            ImGui::Separator();
        }
        if (e.ext == ".json") {
            if (ImGui::MenuItem("Load Scene"))
                if (auto* sm = m_editor->getSceneManager(); sm && sm->loadScene(e.path)) m_editor->log(("Loaded: " + e.name).c_str(), EditorColors::Success);
            ImGui::Separator();
        }
        if (e.ext == ".anim") {
            EditorSelection& sel = m_editor->getSelection();
            bool hasAnimComp = sel.has() && sel.object->getComponent<ComponentAnimation>() != nullptr;
            if (!hasAnimComp) ImGui::BeginDisabled();
            if (ImGui::MenuItem("Assign to Selected")) assignAnimToSelection(e.path);
            if (!hasAnimComp) ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !hasAnimComp)
                ImGui::SetTooltip("Select a GameObject with a ComponentAnimation first");
            ImGui::Separator();
        }
        if (e.ext == ".sm") {
            EditorSelection& sel = m_editor->getSelection();
            bool hasAnimComp = sel.has() && sel.object->getComponent<ComponentAnimation>() != nullptr;
            if (!hasAnimComp) ImGui::BeginDisabled();
            if (ImGui::MenuItem("Load State Machine on Selected")) assignSmToSelection(e.path);
            if (!hasAnimComp) ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !hasAnimComp)
                ImGui::SetTooltip("Select a GameObject with a ComponentAnimation first");
            ImGui::Separator();
        }
        if (e.ext == ".gltf" || e.ext == ".fbx" || e.ext == ".obj") {
            EditorSelection& sel = m_editor->getSelection();
            if (!sel.has()) ImGui::BeginDisabled();
            if (ImGui::MenuItem("Apply Texture / Model to Selection")) spawnAsset(e.path);
            if (!sel.has()) ImGui::EndDisabled();
            ImGui::Separator();
        }
        EditorSelection& sel = m_editor->getSelection();
        if (sel.has()) {
            if (ImGui::MenuItem("Save Selected as Prefab...")) { m_showSavePrefabModal = true; strncpy_s(m_savePrefabNameBuf, sel.object->getName().c_str(), sizeof(m_savePrefabNameBuf) - 1); }
            ImGui::Separator();
        }
        ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Danger);
        if (ImGui::MenuItem("Delete")) {
            app->getFileSystem()->Delete(e.path.c_str());
            m_editor->log(("Deleted: " + e.name).c_str(), EditorColors::Warning);
            if (m_selectedPath == e.path) m_selectedPath.clear();
            m_dirty = true;
        }
        ImGui::PopStyleColor();
    }
    ImGui::EndPopup();
}

void AssetBrowserPanel::drawEmptyAreaContextMenu() {
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered()) ImGui::OpenPopup("##emptyCtx");
    if (!ImGui::BeginPopup("##emptyCtx")) return;
    textMuted("  %s", fs::path(m_currentPath).filename().string().c_str());
    ImGui::Separator();
    if (ImGui::MenuItem("Refresh")) m_dirty = true;
    if (ImGui::MenuItem("Show in Explorer")) ShellExecuteA(nullptr, "explore", m_currentPath.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
    ImGui::Separator();
    EditorSelection& sel = m_editor->getSelection();
    if (sel.has()) {
        if (ImGui::MenuItem("Save Selected as Prefab...")) { m_showSavePrefabModal = true; strncpy_s(m_savePrefabNameBuf, sel.object->getName().c_str(), sizeof(m_savePrefabNameBuf) - 1); }
    }
    else { ImGui::BeginDisabled(); ImGui::MenuItem("Save Selected as Prefab (no selection)"); ImGui::EndDisabled(); }
    ImGui::EndPopup();
}

void AssetBrowserPanel::drawPrefabInstanceBar() {
    EditorSelection& sel = m_editor->getSelection();
    if (!sel.has()) return;
    GameObject* go = sel.object;
    std::string pfName = PrefabManager::getPrefabName(go);
    const PrefabInstanceData* inst = PrefabManager::getInstanceData(go);
    bool hasOverrides = inst && !inst->overrides.isEmpty();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.16f, 0.12f, 1.f));
    ImGui::BeginChild("##instBar", ImVec2(0, 48), false, ImGuiWindowFlags_NoScrollbar);

    ImGui::SetCursorPos({ 6, 3 });
    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Success);
    ImGui::Text("[P]");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 4);
    ImGui::Text("%s", go->getName().c_str());
    ImGui::SameLine(0, 4);
    textMuted("->  %s", pfName.c_str());
    if (hasOverrides) { ImGui::SameLine(0, 4); textActive("(*)"); }

    ImGui::SetCursorPos({ 6, 26 });
    const float bw = 58.f;
    const float pad = 4.f;

    ImGui::BeginDisabled(!hasOverrides);
    ImGui::PushStyleColor(ImGuiCol_Button, hasOverrides ? ImVec4(0.14f, 0.40f, 0.14f, 1.f) : ImVec4(0.15f, 0.15f, 0.15f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.56f, 0.20f, 1.f));
    if (ImGui::SmallButton("Apply##ib")) prefabApply();
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(hasOverrides ? "Push changes back to the prefab file" : "No overrides to apply");

    ImGui::SameLine(0, pad);

    ImGui::BeginDisabled(!hasOverrides);
    if (ImGui::SmallButton("Revert##ib")) prefabRevert();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(hasOverrides ? "Restore from prefab file" : "No overrides to revert");

    ImGui::SameLine(0, pad);

    ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::Warning);
    if (ImGui::SmallButton("Unlink##ib")) {
        PrefabManager::unlinkInstance(go);
        m_editor->log(("Unlinked '" + go->getName() + "'").c_str(), EditorColors::Warning);
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Break prefab connection (always available)");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AssetBrowserPanel::drawStatusBar() {
    const float totalW = ImGui::GetContentRegionAvail().x;
    const float bw1 = 90.0f, bw2 = 54.0f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);

    if (!m_selectedPath.empty() && !fs::is_directory(m_selectedPath)) {
        std::string fname = fs::path(m_selectedPath).filename().string();
        std::string ext = fs::path(m_selectedPath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        std::string stem = fs::path(m_selectedPath).stem().string();

        ImGui::PushStyleColor(ImGuiCol_Text, typeColor(ext));
        ImGui::Text(" %s", typeIcon(ext));
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);

        float nameMaxW = totalW - bw1 - bw2 - 60.0f - 12.0f;
        while (stem.size() > 3 && ImGui::CalcTextSize(stem.c_str()).x > nameMaxW) stem.resize(stem.size() - 1);
        if (stem != fs::path(m_selectedPath).stem().string()) stem += "..";
        ImGui::TextUnformatted(stem.c_str());

        ImGui::SameLine();
        ImGui::SetCursorPosX(totalW - bw1 - bw2 - 4);
        if (ImGui::Button("Add to Scene", ImVec2(bw1, 0))) spawnAsset(m_selectedPath);
        ImGui::SameLine(0, 4);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.15f, 0.15f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.20f, 0.20f, 1.f));
        if (ImGui::Button("Delete", ImVec2(bw2, 0))) {
            app->getFileSystem()->Delete(m_selectedPath.c_str());
            m_editor->log(("Deleted: " + fname).c_str(), EditorColors::Warning);
            m_selectedPath.clear(); m_dirty = true;
        }
        ImGui::PopStyleColor(2);
    }
    else {
        int fc = 0; for (const auto& e : m_entries) if (!e.isDir) fc++;
        textMuted("  %d asset(s)   \x97   %s", fc, fs::path(m_currentPath).filename().string().c_str());
    }
}

void AssetBrowserPanel::drawModals() {
    ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    auto openModal = [&](bool& flag, const char* id) {
        if (flag) { ImGui::OpenPopup(id); flag = false; }
        ImGui::SetNextWindowPos(centre, ImGuiCond_Appearing, { 0.5f, 0.5f });
        };

    openModal(m_showVariantModal, "##variantModal");
    ImGui::SetNextWindowSize({ 320, 0 });
    if (ImGui::BeginPopupModal("##variantModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create variant of:  \"%s\"", m_variantSrcBuf);
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("Variant name##vname", m_variantDstBuf, sizeof(m_variantDstBuf));
        ImGui::Spacing();
        if (ImGui::Button("Create", { 100, 0 })) { prefabCreateVariant(m_variantSrcBuf, m_variantDstBuf); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", { 80, 0 })) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    openModal(m_renamingPrefab, "##renameModal");
    ImGui::SetNextWindowSize({ 300, 0 });
    if (ImGui::BeginPopupModal("##renameModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Rename prefab:  \"%s\"", m_renameSrcBuf);
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1);
        bool enter = ImGui::InputText("New name##rname", m_renameDstBuf, sizeof(m_renameDstBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();
        if (ImGui::Button("Rename", { 100, 0 }) || enter) { prefabRename(m_renameSrcBuf, m_renameDstBuf); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", { 80, 0 })) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    openModal(m_showSavePrefabModal, "##savePfModal");
    ImGui::SetNextWindowSize({ 300, 0 });
    if (ImGui::BeginPopupModal("##savePfModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        EditorSelection& sel = m_editor->getSelection();
        ImGui::Text("Save \"%s\" as prefab:", sel.has() ? sel.object->getName().c_str() : "?");
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1);
        bool enter = ImGui::InputText("Prefab name##pfn", m_savePrefabNameBuf, sizeof(m_savePrefabNameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();
        if ((ImGui::Button("Save", { 100, 0 }) || enter) && strlen(m_savePrefabNameBuf) > 0) { prefabSaveSelected(m_savePrefabNameBuf); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", { 80, 0 })) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::drawColoredBox(ImVec2 lp, ImVec4 col, const char* label) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 tl = { wp.x + lp.x - ImGui::GetScrollX(), wp.y + lp.y - ImGui::GetScrollY() };
    ImVec2 br = { tl.x + kThumbSize, tl.y + kThumbSize };
    dl->AddRectFilled(tl, br, EditorColors::toU32Scaled(col, 0.20f), 6.f);
    dl->AddRect(tl, br, EditorColors::toU32Scaled(col, 0.65f), 6.f, 0, 1.5f);
    if (label && label[0]) {
        ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText({ tl.x + (kThumbSize - ts.x) * 0.5f, tl.y + (kThumbSize - ts.y) * 0.5f }, EditorColors::toU32(col), label);
    }
}

void AssetBrowserPanel::spawnAsset(const std::string& path) {
    if (path.empty() || fs::is_directory(path)) return;
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    ModuleScene* scene = m_editor->getActiveModuleScene();
    EditorSelection& sel = m_editor->getSelection();

    if (ext == ".gltf" || ext == ".fbx" || ext == ".obj") {
        if (!scene) return;
        std::string name = fs::path(path).stem().string();
        GameObject* go = scene->createGameObject(name.c_str());
        bool ok = go->createComponent<ComponentMesh>()->loadModel(path.c_str());
        m_editor->log(ok ? ("Added: " + name).c_str() : ("Failed: " + path).c_str(), ok ? EditorColors::Success : EditorColors::Danger);
        if (ok) sel.object = go;
    }
    else if (ext == ".json") {
        if (auto* sm = m_editor->getSceneManager(); sm && sm->loadScene(path)) m_editor->log(("Loaded scene: " + path).c_str(), EditorColors::Success);
    }
    else if (ext == ".prefab") prefabInstantiate(fs::path(path).stem().string());
    else if (ext == ".dds" || ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
        auto& th = m_thumbCache[path];
        if (!th.attempted) { th.attempted = true; TextureImporter::Load(path, th.tex, th.srv); }
        if (!th.tex) { m_editor->log("Failed to load texture.", EditorColors::Danger); return; }
        std::string stem = fs::path(path).stem().string();
        if (scene) { sel.object = PrimitiveFactory::createTexturedQuadObject(scene, stem, th.tex, th.srv); m_editor->log(("Added image: " + stem).c_str(), EditorColors::Success); }
    }
}


void AssetBrowserPanel::prefabSaveSelected(const char* name) {
    EditorSelection& sel = m_editor->getSelection();
    if (!sel.has()) { m_editor->log("No GameObject selected.", EditorColors::Danger); return; }
    if (PrefabManager::createPrefab(sel.object, name)) {
        PrefabInstanceData d; d.prefabName = name; d.prefabUID = PrefabManager::getPrefabUID(sel.object);
        PrefabManager::linkInstance(sel.object, d);
        m_editor->log(("Prefab saved: " + std::string(name)).c_str(), EditorColors::Success);
        m_dirty = true;
    }
    else m_editor->log("Prefab save failed.", EditorColors::Danger);
}

void AssetBrowserPanel::prefabInstantiate(const std::string& name) {
    ModuleScene* scene = m_editor->getActiveModuleScene();
    if (!scene) { m_editor->log("No active scene.", EditorColors::Danger); return; }
    if (GameObject* go = PrefabManager::instantiatePrefab(name, scene)) {
        m_editor->getSelection().object = go;
        m_editor->log(("Instantiated: " + name).c_str(), EditorColors::Success);
    }
    else m_editor->log(("Failed: " + name).c_str(), EditorColors::Danger);
}

void AssetBrowserPanel::prefabApply() {
    EditorSelection& sel = m_editor->getSelection();
    if (!sel.has()) return;
    bool ok = PrefabManager::applyToPrefab(sel.object, true);
    logResult(m_editor, ok, "Applied to prefab.", "Apply failed.");
}

void AssetBrowserPanel::prefabRevert() {
    EditorSelection& sel = m_editor->getSelection();
    if (!sel.has()) return;
    bool ok = PrefabManager::revertToPrefab(sel.object, m_editor->getActiveModuleScene());
    logResult(m_editor, ok, "Reverted.", "Revert failed.");
}

void AssetBrowserPanel::prefabDelete(const std::string& name) {
    std::string prefabDir = app->getFileSystem()->GetLibraryPath() + "Prefabs/";
    app->getFileSystem()->Delete((prefabDir + name + ".prefab").c_str());
    m_editor->log(("Deleted prefab: " + name).c_str(), EditorColors::Warning);
    m_dirty = true;
}

void AssetBrowserPanel::prefabCreateVariant(const std::string& src, const std::string& dst) {
    if (dst.empty()) return;
    bool ok = PrefabManager::createVariant(src, dst);
    logResult(m_editor, ok, ("Variant: " + dst).c_str(), "Variant failed.");
    m_dirty = true;
}

void AssetBrowserPanel::prefabRename(const std::string& oldName, const std::string& newName) {
    if (newName.empty() || newName == oldName) return;
    std::string prefabDir = app->getFileSystem()->GetLibraryPath() + "Prefabs/";
    std::string oldPath = prefabDir + oldName + ".prefab";
    std::string newPath = prefabDir + newName + ".prefab";
    app->getFileSystem()->Copy(oldPath.c_str(), newPath.c_str());
    app->getFileSystem()->Delete(oldPath.c_str());
    m_editor->log(("Renamed: " + oldName + " -> " + newName).c_str(), EditorColors::Success);
    m_dirty = true;
}

void AssetBrowserPanel::assignAnimToSelection(const std::string& animPath) {
    EditorSelection& sel = m_editor->getSelection();
    if (!sel.has()) { m_editor->log("No GameObject selected.", EditorColors::Danger); return; }
    auto* anim = sel.object->getComponent<ComponentAnimation>();
    if (!anim) { m_editor->log("Selected object has no ComponentAnimation.", EditorColors::Danger); return; }

    // Find or request the animation resource by path
    UID uid = app->getAssets()->findUID(animPath);
    if (uid == 0) {
        // It's a Library .anim file — look it up by path directly in the resource registry
        uid = app->getAssets()->findUID(animPath);
    }

    ResourceAnimation* res = nullptr;
    if (uid != 0) {
        res = app->getResources()->RequestAnimation(uid);
    }
    else {
        // Fallback: load directly (Library files may not have a top-level UID)
        static ResourceAnimation tempAnim(0);
        tempAnim.libraryFile = animPath;
        if (tempAnim.LoadInMemory()) res = &tempAnim;
    }

    if (!res) { m_editor->log(("Failed to load anim: " + animPath).c_str(), EditorColors::Danger); return; }

    std::string clipName = fs::path(animPath).stem().string();
    anim->registerClip(clipName, res);
    m_editor->log(("Registered clip '" + clipName + "' on " + sel.object->getName()).c_str(), EditorColors::Success);
}

void AssetBrowserPanel::assignSmToSelection(const std::string& smPath) {
    EditorSelection& sel = m_editor->getSelection();
    if (!sel.has()) { m_editor->log("No GameObject selected.", EditorColors::Danger); return; }
    auto* anim = sel.object->getComponent<ComponentAnimation>();
    if (!anim) { m_editor->log("Selected object has no ComponentAnimation.", EditorColors::Danger); return; }

    // Allocate and load the state machine (editor owns the lifetime here)
    auto* smRes = new StateMachineResource();
    if (!smRes->loadFromFile(smPath)) {
        delete smRes;
        m_editor->log(("Failed to load state machine: " + smPath).c_str(), EditorColors::Danger);
        return;
    }

    anim->setStateMachineResource(smRes);
    m_editor->log(("Loaded SM '" + fs::path(smPath).stem().string() + "' on " + sel.object->getName()).c_str(), EditorColors::Success);
}