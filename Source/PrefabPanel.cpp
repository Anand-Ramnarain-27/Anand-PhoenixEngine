#include "Globals.h"
#include "PrefabPanel.h"
#include "PrefabManager.h"
#include "ModuleEditor.h"
#include "ModuleScene.h"
#include "Application.h"
#include "GameObject.h"
#include "EditorSelection.h"

#include <imgui.h>
#include <filesystem>

namespace fs = std::filesystem;

static const ImVec4 kColActive{ 0.26f, 0.59f, 0.98f, 1.00f };
static const ImVec4 kColSuccess{ 0.40f, 0.85f, 0.40f, 1.00f };
static const ImVec4 kColWarn{ 0.95f, 0.75f, 0.20f, 1.00f };
static const ImVec4 kColDanger{ 0.90f, 0.30f, 0.30f, 1.00f };
static const ImVec4 kColMuted{ 0.50f, 0.50f, 0.50f, 1.00f };
static const ImVec4 kColVariant{ 0.70f, 0.50f, 0.95f, 1.00f };
static const ImVec4 kColOverride{ 0.95f, 0.65f, 0.15f, 1.00f };

void PrefabPanel::draw()
{
    ImGui::Begin("Prefabs", &open);

    const float fullH = ImGui::GetContentRegionAvail().y;
    const float bottomH = 130.0f;   
    const float listH = fullH - bottomH - 8.0f;

    drawToolbar();
    ImGui::Separator();
    drawPrefabList(listH);
    ImGui::Separator();
    drawInstanceControls();

    if (m_showVariantPopup)
    {
        ImGui::OpenPopup("Create Variant");
        m_showVariantPopup = false;
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Create Variant", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Fork '%s' into a new variant:", m_selectedPrefab.c_str());
        ImGui::SetNextItemWidth(240.0f);
        ImGui::InputText("Variant name##vname", m_variantNameBuf, sizeof(m_variantNameBuf));
        ImGui::Spacing();
        if (ImGui::Button("Create", ImVec2(110, 0)))
        {
            doCreateVariant(m_selectedPrefab, m_variantNameBuf);
            memset(m_variantNameBuf, 0, sizeof(m_variantNameBuf));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::End();
}

void PrefabPanel::drawToolbar()
{
    // Search box
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##pfSearch", "Filter prefabs",
        m_searchBuf, sizeof(m_searchBuf));
}

void PrefabPanel::drawPrefabList(float listH)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.11f, 0.11f, 1.0f));
    ImGui::BeginChild("##PFList", ImVec2(0, listH), false);

    std::string filterLow(m_searchBuf);
    std::transform(filterLow.begin(), filterLow.end(), filterLow.begin(), ::tolower);

    auto prefabs = PrefabManager::listPrefabsInfo();

    if (prefabs.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kColMuted);
        ImGui::SetCursorPosX(12);
        ImGui::TextWrapped("No prefabs yet. Select a GameObject and use 'Save as Prefab' below.");
        ImGui::PopStyleColor();
    }

    for (const auto& info : prefabs)
    {
        if (!filterLow.empty())
        {
            std::string nameLow = info.name;
            std::transform(nameLow.begin(), nameLow.end(), nameLow.begin(), ::tolower);
            if (nameLow.find(filterLow) == std::string::npos) continue;
        }

        bool selected = (m_selectedPrefab == info.name);

        ImGui::PushID(info.name.c_str());

        ImGui::PushStyleColor(ImGuiCol_Header,
            ImVec4(0.26f, 0.59f, 0.98f, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
            ImVec4(0.26f, 0.59f, 0.98f, 0.20f));

        bool clicked = ImGui::Selectable("##row", selected,
            ImGuiSelectableFlags_SpanAllColumns
            | ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0, 36));

        if (clicked) m_selectedPrefab = info.name;

        if (clicked && ImGui::IsMouseDoubleClicked(0))
            doInstantiate(info.name);

        ImGui::PopStyleColor(2);

        float rowY = ImGui::GetItemRectMin().y;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cp = ImGui::GetCursorPos();

        ImGui::SameLine(8.0f);
        ImGui::SetCursorPosY(rowY - ImGui::GetWindowPos().y + 6.0f + ImGui::GetScrollY());

        ImGui::PushStyleColor(ImGuiCol_Text, info.isVariant ? kColVariant : kColActive);
        ImGui::Text(info.isVariant ? "[V]" : "[P]");
        ImGui::PopStyleColor();

        ImGui::SameLine(40.0f);

        if (m_renaming && selected)
        {
            ImGui::SetNextItemWidth(130.0f);
            if (ImGui::InputText("##rename", m_renameBuf, sizeof(m_renameBuf),
                ImGuiInputTextFlags_EnterReturnsTrue
                | ImGuiInputTextFlags_AutoSelectAll))
            {
                std::string oldPath = "Library/Prefabs/" + info.name + ".prefab";
                std::string newPath = "Library/Prefabs/" + std::string(m_renameBuf) + ".prefab";
                if (strlen(m_renameBuf) > 0 && m_renameBuf != info.name)
                {
                    app->getFileSystem()->Copy(oldPath.c_str(), newPath.c_str());
                    app->getFileSystem()->Delete(oldPath.c_str());
                    m_selectedPrefab = m_renameBuf;
                    m_editor->log(("Prefab renamed -> " + std::string(m_renameBuf)).c_str(),
                        kColSuccess);
                }
                m_renaming = false;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) m_renaming = false;
        }
        else
        {
            ImGui::Text("%s", info.name.c_str());
            ImGui::SameLine();

            if (info.isVariant)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, kColVariant);
                ImGui::Text("  variant of: %s", info.variantOf.c_str());
                ImGui::PopStyleColor();
            }
        }

        ImGui::SetCursorPosX(40.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, kColMuted);
        if (info.childCount > 0)
            ImGui::Text("%s  |  %d child%s", info.componentSummary.c_str(),
                info.childCount, info.childCount == 1 ? "" : "ren");
        else
            ImGui::Text("%s", info.componentSummary.c_str());
        ImGui::PopStyleColor();

        if (ImGui::BeginPopupContextItem("##pfCtx"))
        {
            m_selectedPrefab = info.name;

            if (ImGui::MenuItem("Instantiate"))          doInstantiate(info.name);
            ImGui::Separator();
            if (ImGui::MenuItem("Rename"))
            {
                m_renaming = true;
                strncpy_s(m_renameBuf, info.name.c_str(), sizeof(m_renameBuf) - 1);
            }
            if (ImGui::MenuItem("Create Variant"))
            {
                m_showVariantPopup = true;
                strncpy_s(m_variantNameBuf,
                    (info.name + "_variant").c_str(),
                    sizeof(m_variantNameBuf) - 1);
            }
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, kColDanger);
            if (ImGui::MenuItem("Delete"))               doDelete(info.name);
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void PrefabPanel::drawInstanceControls()
{
    EditorSelection& sel = m_editor->getSelection();
    GameObject* go = sel.has() ? sel.object : nullptr;
    bool             isInst = go && PrefabManager::isPrefabInstance(go);

    drawCreateFromSelection();

    ImGui::Spacing();

    if (!isInst)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kColMuted);
        ImGui::TextWrapped("Select a prefab instance to Apply / Revert.");
        ImGui::PopStyleColor();
        return;
    }

    std::string pfName = PrefabManager::getPrefabName(go);
    const PrefabInstanceData* instData = PrefabManager::getInstanceData(go);

    ImGui::PushStyleColor(ImGuiCol_Text, kColActive);
    ImGui::Text("[Instance] %s  ->  %s", go->getName().c_str(), pfName.c_str());
    ImGui::PopStyleColor();

    if (instData && !instData->overrides.isEmpty())
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, kColOverride);
        ImGui::Text("  (overrides)");
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    const float btnW = 100.0f;

    if (ImGui::Button("Apply", ImVec2(btnW, 0)))
        doApply();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Push instance changes back to the prefab file on disk.\n"
            "Local override records are preserved.");
    ImGui::SameLine();

    if (ImGui::Button("Revert", ImVec2(btnW, 0)))
        doRevert();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Restore non-overridden properties from the prefab file.\n"
            "Overridden properties are left untouched.");
    ImGui::SameLine();

    bool hasOverrides = instData && !instData->overrides.isEmpty();
    if (!hasOverrides) ImGui::BeginDisabled();
    if (ImGui::Button("Clear Overrides", ImVec2(130, 0)))
    {
        PrefabManager::clearAllOverrides(go);
        m_editor->log("Overrides cleared.", kColWarn);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Remove all override records.\n"
            "The next Revert will restore everything.");
    if (!hasOverrides) ImGui::EndDisabled();
    ImGui::SameLine();

    // Unlink
    if (ImGui::Button("Unlink", ImVec2(70, 0)))
    {
        PrefabManager::unlinkInstance(go);
        m_editor->log(("Unlinked '" + go->getName() + "' from prefab.").c_str(), kColWarn);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Break the prefab connection.\n"
            "The GameObject becomes a standalone object.");
}

void PrefabPanel::drawCreateFromSelection()
{
    EditorSelection& sel = m_editor->getSelection();
    GameObject* go = sel.has() ? sel.object : nullptr;
    bool             hasGO = (go != nullptr);

    if (!hasGO) ImGui::BeginDisabled();

    ImGui::SetNextItemWidth(-80.0f);
    ImGui::InputTextWithHint("##newpfname",
        hasGO ? go->getName().c_str() : "Prefab name",
        m_newPrefabNameBuf,
        sizeof(m_newPrefabNameBuf));
    ImGui::SameLine();
    if (ImGui::Button("Save as Prefab", ImVec2(-1, 0)))
    {
        std::string name = (strlen(m_newPrefabNameBuf) > 0)
            ? std::string(m_newPrefabNameBuf)
            : go->getName();
        doCreate(name);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Serialise the selected GameObject (and all its children)\n"
            "to Library/Prefabs/<name>.prefab");

    if (!hasGO) ImGui::EndDisabled();
}

void PrefabPanel::doCreate(const std::string& name)
{
    EditorSelection& sel = m_editor->getSelection();
    if (!sel.has()) { m_editor->log("No GameObject selected.", kColDanger); return; }

    if (PrefabManager::createPrefab(sel.object, name))
    {
        PrefabInstanceData data;
        data.prefabName = name;
        data.prefabUID = PrefabManager::getPrefabUID(sel.object);
        if (data.prefabUID == 0) data.prefabUID = 0; 
        PrefabManager::linkInstance(sel.object, data);

        m_selectedPrefab = name;
        memset(m_newPrefabNameBuf, 0, sizeof(m_newPrefabNameBuf));
        m_editor->log(("Prefab saved: " + name).c_str(), kColSuccess);
    }
    else
    {
        m_editor->log(("Failed to save prefab: " + name).c_str(), kColDanger);
    }
}

void PrefabPanel::doInstantiate(const std::string& name)
{
    ModuleScene* scene = m_editor->getActiveModuleScene();
    if (!scene) { m_editor->log("No active scene.", kColDanger); return; }

    GameObject* go = PrefabManager::instantiatePrefab(name, scene);
    if (go)
    {
        m_editor->getSelection().object = go;
        m_editor->log(("Instantiated: " + name).c_str(), kColSuccess);
    }
    else
    {
        m_editor->log(("Failed to instantiate: " + name).c_str(), kColDanger);
    }
}

void PrefabPanel::doApply()
{
    EditorSelection& sel = m_editor->getSelection();
    if (!sel.has()) return;

    bool ok = PrefabManager::applyToPrefab(sel.object, /*respectOverrides=*/true);
    m_editor->log(ok ? "Applied to prefab." : "Apply failed.", ok ? kColSuccess : kColDanger);
}

void PrefabPanel::doRevert()
{
    EditorSelection& sel = m_editor->getSelection();
    if (!sel.has()) return;

    ModuleScene* scene = m_editor->getActiveModuleScene();
    bool ok = PrefabManager::revertToPrefab(sel.object, scene);
    m_editor->log(ok ? "Reverted from prefab." : "Revert failed.", ok ? kColSuccess : kColDanger);
}

void PrefabPanel::doDelete(const std::string& name)
{
    std::string path = "Library/Prefabs/" + name + ".prefab";
    app->getFileSystem()->Delete(path.c_str());
    if (m_selectedPrefab == name) m_selectedPrefab.clear();
    m_editor->log(("Deleted prefab: " + name).c_str(), kColWarn);
}

void PrefabPanel::doCreateVariant(const std::string& srcName, const std::string& dstName)
{
    if (dstName.empty()) return;
    bool ok = PrefabManager::createVariant(srcName, dstName);
    m_editor->log(ok ? ("Variant created: " + dstName).c_str()
        : "Variant creation failed.", ok ? kColSuccess : kColDanger);
    if (ok) m_selectedPrefab = dstName;
}