#include "Globals.h"
#include "AssetBrowserPanel.h"
#include "ModuleAssets.h"
#include "ModuleEditor.h"
#include "Application.h"
#include "ModuleScene.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "ComponentMesh.h"
#include "PrefabManager.h"
#include "TextureImporter.h"
#include "PrimitiveFactory.h"
#include "EditorSelection.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

void AssetBrowserPanel::draw()
{
    ImGui::Begin("Asset Browser", &open);

    const float panelW = 180.0f;
    const float actionBarH = 28.0f;
    const float panelH = ImGui::GetContentRegionAvail().y - actionBarH - 8.0f;
    const float contentW = ImGui::GetContentRegionAvail().x - panelW - 6.0f;

    drawToolbar();
    ImGui::Separator();
    drawImportSidebar(panelW, panelH);
    ImGui::SameLine(0, 6);
    drawAssetList(contentW, panelH);
    ImGui::Separator();
    drawActionBar();

    ImGui::End();
}

void AssetBrowserPanel::drawToolbar()
{
    const char* filters[] = { "All", "Models", "Textures", "Scenes", "Prefabs" };

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    ImGui::BeginChild("##ABToolbar", ImVec2(0, 32), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos({ 6, 6 });
    for (int i = 0; i < 5; i++)
    {
        bool active = (m_filter == i);
        ImGui::PushStyleColor(ImGuiCol_Button, active
            ? ImVec4(0.26f, 0.59f, 0.98f, 1.0f)
            : ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
        if (ImGui::Button(filters[i], ImVec2(60, 20))) m_filter = i;
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 2);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AssetBrowserPanel::drawImportSidebar(float panelW, float panelH)
{
    const float halfBtn = (panelW - 20) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
    ImGui::BeginChild("##ABLeft", ImVec2(panelW, panelH));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::SetCursorPosX(8); ImGui::Text("IMPORT");
    ImGui::PopStyleColor();
    ImGui::Separator(); ImGui::Spacing();

    if (m_filter == 0 || m_filter == 1)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::SetCursorPosX(8); ImGui::Text("Model (.gltf/.fbx)");
        ImGui::PopStyleColor();

        static char importModelBuf[256] = "Assets/Models/";
        ImGui::SetNextItemWidth(panelW - 16);
        ImGui::SetCursorPosX(8); ImGui::InputText("##impModel", importModelBuf, sizeof(importModelBuf));
        ImGui::SetCursorPosX(8);
        if (ImGui::Button("Browse##model", ImVec2(halfBtn, 0)))
            m_modelBrowseDialog.open(FileDialog::Type::Open, "Select Model", "Assets/Models");
        ImGui::SameLine(0, 4);
        if (ImGui::Button("Import##model", ImVec2(halfBtn, 0)) && strlen(importModelBuf) > 0)
        {
            app->getAssets()->importAsset(importModelBuf);
            m_editor->log(("Imported: " + std::string(importModelBuf)).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
        }
        if (m_modelBrowseDialog.draw())
            strncpy_s(importModelBuf, m_modelBrowseDialog.getSelectedPath().c_str(), sizeof(importModelBuf) - 1);
        ImGui::Spacing();
    }

    if (m_filter == 0 || m_filter == 2)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::SetCursorPosX(8); ImGui::Text("Texture (.png/.dds)");
        ImGui::PopStyleColor();

        static char importTexBuf[256] = "Assets/Textures/";
        ImGui::SetNextItemWidth(panelW - 16);
        ImGui::SetCursorPosX(8); ImGui::InputText("##impTex", importTexBuf, sizeof(importTexBuf));
        ImGui::SetCursorPosX(8);
        if (ImGui::Button("Browse##tex", ImVec2(halfBtn, 0)))
            m_texBrowseDialog.open(FileDialog::Type::Open, "Select Texture", "Assets/Textures");
        ImGui::SameLine(0, 4);
        if (ImGui::Button("Import##tex", ImVec2(halfBtn, 0)) && strlen(importTexBuf) > 0)
        {
            std::string name = TextureImporter::GetTextureName(importTexBuf);
            std::string outDir = app->getFileSystem()->GetLibraryPath() + "Textures/";
            app->getFileSystem()->CreateDir(outDir.c_str());
            bool ok = TextureImporter::Import(importTexBuf, outDir + name + ".dds");
            m_editor->log(ok ? ("Imported texture: " + name).c_str()
                : ("Failed: " + std::string(importTexBuf)).c_str(),
                ok ? ImVec4(0.6f, 1, 0.6f, 1) : ImVec4(1, 0.4f, 0.4f, 1));
        }
        if (m_texBrowseDialog.draw())
            strncpy_s(importTexBuf, m_texBrowseDialog.getSelectedPath().c_str(), sizeof(importTexBuf) - 1);
        ImGui::Spacing();
    }

    if (m_filter == 0 || m_filter == 3)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::SetCursorPosX(8); ImGui::Text("Scene");
        ImGui::PopStyleColor();

        SceneManager* sm = m_editor->getSceneManager();
        ImGui::SetCursorPosX(8);
        if (ImGui::Button("Quick Save", ImVec2(panelW - 16, 0)) && sm)
        {
            std::string p = "Library/Scenes/current_scene.json";
            if (sm->saveCurrentScene(p)) m_editor->log(("Saved: " + p).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
            else                         m_editor->log("Failed to save.", ImVec4(1, 0.4f, 0.4f, 1));
        }
        ImGui::Spacing();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AssetBrowserPanel::drawAssetList(float contentW, float panelH)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.11f, 0.11f, 1.0f));
    ImGui::BeginChild("##ABRight", ImVec2(contentW, panelH));
    ImGui::Spacing();

    static char searchBuf[128] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##absearch", "Search...", searchBuf, sizeof(searchBuf));
    ImGui::Separator();

    std::string search(searchBuf);
    std::transform(search.begin(), search.end(), search.begin(), ::tolower);

    auto sectionHeader = [](const char* label)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::Text("  %s", label);
            ImGui::PopStyleColor();
            ImGui::Separator();
        };

    auto emptyMsg = [](const char* msg)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
            ImGui::Text("    %s", msg);
            ImGui::PopStyleColor();
        };

    if (m_filter == 0 || m_filter == 1)
    {
        sectionHeader("MODELS");
        std::string modPath = app->getFileSystem()->GetLibraryPath() + "Meshes/";
        bool any = false;
        try {
            for (const auto& e : fs::directory_iterator(modPath))
            {
                if (!e.is_directory()) continue;

                std::string name = e.path().filename().string();
                std::string sourcePath = "Assets/Models/" + name + "/" + name + ".gltf";
                UID uid = app->getAssets()->findUID(sourcePath);

                std::string uidStr = uid ? ("uid:" + std::to_string(uid)).substr(0, 16) : "no uid";
                drawAssetRow(e.path().string(), "[M]", "model", uidStr);
                any = true;
            }
        }
        catch (...) {}
        if (!any) emptyMsg("No models imported yet.");
        ImGui::Spacing();
    }

    if (m_filter == 0 || m_filter == 2)
    {
        sectionHeader("TEXTURES");
        auto files = app->getFileSystem()->GetFilesInDirectory(
            (app->getFileSystem()->GetLibraryPath() + "Textures/").c_str(), ".dds");
        if (files.empty()) emptyMsg("No textures imported yet.");
        else for (const auto& f : files) drawAssetRow(f, "[T]", "texture");
        ImGui::Spacing();
    }

    if (m_filter == 0 || m_filter == 3)
    {
        sectionHeader("SCENES");
        auto files = app->getFileSystem()->GetFilesInDirectory(
            (app->getFileSystem()->GetLibraryPath() + "Scenes/").c_str(), ".json");
        if (files.empty()) emptyMsg("No scenes saved yet.");
        else
        {
            for (const auto& f : files)
            {
                bool dbl = drawAssetRow(f, "[S]", "scene");
                if (dbl && ImGui::IsMouseDoubleClicked(0))
                {
                    SceneManager* sm = m_editor->getSceneManager();
                    if (sm && sm->loadScene(f))
                        m_editor->log(("Loaded: " + f).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
                }
                if (ImGui::BeginPopupContextItem(("##sc" + f).c_str()))
                {
                    SceneManager* sm = m_editor->getSceneManager();
                    if (ImGui::MenuItem("Load") && sm) sm->loadScene(f);
                    if (ImGui::MenuItem("Delete")) app->getFileSystem()->Delete(f.c_str());
                    ImGui::EndPopup();
                }
            }
        }
        ImGui::Spacing();
    }

    if (m_filter == 0 || m_filter == 4)
    {
        sectionHeader("PREFABS");
        auto prefabs = PrefabManager::listPrefabs();
        if (prefabs.empty()) emptyMsg("No prefabs yet.");
        else for (const auto& p : prefabs) drawAssetRow(p, "[P]", "prefab");
        ImGui::Spacing();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

bool AssetBrowserPanel::drawAssetRow(const std::string& path, const std::string& icon,
    const std::string& type, const std::string& extra)
{
    static int rowIdx = 0;
    bool selected = (m_selectedPath == path);

    ImGui::PushID(path.c_str());
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.26f, 0.59f, 0.98f, 0.40f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.25f));

    bool clicked = ImGui::Selectable("##row", selected,
        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick,
        ImVec2(0, 20));

    ImGui::PopStyleColor(2);

    if (clicked) { m_selectedPath = path; m_selectedType = type; }

    ImGui::SameLine(8);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
    ImGui::Text("%s", icon.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine(36);
    ImGui::Text("%s", fs::path(path).filename().string().c_str());
    if (!extra.empty())
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1.0f));
        ImGui::Text("  %s", extra.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PopID();
    return clicked;
}

void AssetBrowserPanel::drawActionBar()
{
    bool hasSelection = !m_selectedPath.empty();

    if (hasSelection)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
        ImGui::Text("Selected: %s", fs::path(m_selectedPath).filename().string().c_str());
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        ImGui::Text("No asset selected");
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    if (!hasSelection) ImGui::BeginDisabled();

    float btnX = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + btnX - 220);

    if (ImGui::Button("Add to Scene", ImVec2(110, 0)) && hasSelection)
        handleAddToScene();

    ImGui::SameLine(0, 4);
    if (ImGui::Button("Delete", ImVec2(100, 0)) && hasSelection)
    {
        if (m_selectedType == "scene" || m_selectedType == "texture")
        {
            app->getFileSystem()->Delete(m_selectedPath.c_str());
            m_editor->log(("Deleted: " + m_selectedPath).c_str(), ImVec4(1, 0.6f, 0.4f, 1));
            m_selectedPath.clear();
            m_selectedType.clear();
        }
    }

    if (!hasSelection) ImGui::EndDisabled();
}

void AssetBrowserPanel::handleAddToScene()
{
    ModuleScene* scene = m_editor->getActiveModuleScene();
    EditorSelection& sel = m_editor->getSelection();
    if (!scene) return;

    if (m_selectedType == "model")
    {
        std::string name = fs::path(m_selectedPath).filename().string();
        std::string gltf = "Assets/Models/" + name + "/" + name + ".gltf";
        GameObject* go = scene->createGameObject(name);
        auto* cm = go->createComponent<ComponentMesh>();
        bool ok = cm->loadModel(gltf.c_str());
        m_editor->log(ok ? ("Added: " + name).c_str() : ("Failed: " + gltf).c_str(),
            ok ? ImVec4(0.6f, 1, 0.6f, 1) : ImVec4(1, 0.4f, 0.4f, 1));
        sel.object = go;
    }
    else if (m_selectedType == "texture")
    {
        if (m_pendingTexturePath != m_selectedPath)
        {
            m_pendingTexture.Reset();
            if (!TextureImporter::Load(m_selectedPath, m_pendingTexture, m_pendingTextureSRV))
            {
                m_editor->log("Failed to load texture.", ImVec4(1, 0.4f, 0.4f, 1));
                m_pendingTexturePath.clear();
                return;
            }
            m_pendingTexturePath = m_selectedPath;
        }

        std::string stem = fs::path(m_selectedPath).stem().string();
        if (sel.has())
        {
            auto* existing = sel.object->getComponent<ComponentMesh>();
            if (existing && existing->getModel())
            {
                for (auto& mat : existing->getModel()->getMaterials())
                    mat->setBaseColorTexture(m_pendingTexture, m_pendingTextureSRV);
                existing->rebuildMaterialBuffers();
                m_editor->log(("Applied texture to: " + sel.object->getName()).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
            }
            else
            {
                if (!existing) existing = sel.object->createComponent<ComponentMesh>();
                existing->setModel(PrimitiveFactory::createTexturedQuad(m_pendingTexture, m_pendingTextureSRV));
                existing->rebuildMaterialBuffers();
                m_editor->log(("Created quad on: " + sel.object->getName()).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
            }
        }
        else
        {
            sel.object = PrimitiveFactory::createTexturedQuadObject(scene, stem, m_pendingTexture, m_pendingTextureSRV);
            m_editor->log(("Added image: " + stem).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
        }
    }
    else if (m_selectedType == "scene")
    {
        SceneManager* sm = m_editor->getSceneManager();
        if (sm && sm->loadScene(m_selectedPath))
            m_editor->log(("Loaded scene: " + m_selectedPath).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
    }
    else if (m_selectedType == "prefab")
    {
        PrefabManager::instantiatePrefab(fs::path(m_selectedPath).stem().string(), scene);
        m_editor->log(("Instantiated: " + m_selectedPath).c_str(), ImVec4(0.6f, 1, 0.6f, 1));
    }
}