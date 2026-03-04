#include "Globals.h"
#include "ResourcesPanel.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleAssets.h"
#include "ResourceBase.h"
#include <imgui.h>

static const char* typeName(ResourceBase::Type t)
{
    switch (t)
    {
    case ResourceBase::Type::Mesh:    return "Mesh";
    case ResourceBase::Type::Texture: return "Texture";
    case ResourceBase::Type::Model:   return "Model";
    case ResourceBase::Type::Scene:   return "Scene";
    default:                          return "Unknown";
    }
}

void ResourcesPanel::draw()
{
    ImGui::Begin("Resources", &open);

    const auto& resources = app->getResources()->getLoadedResources();

    ImGui::Text("Resources in memory: %d", (int)resources.size());
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.f));
    ImGui::Text("  %-16s %-10s %-5s  %s", "UID", "Type", "Refs", "Asset Path");
    ImGui::PopStyleColor();
    ImGui::Separator();

    for (auto& [uid, res] : resources)
    {
        std::string path = app->getAssets()->getPathFromUID(uid);

        ImVec4 col = res->referenceCount > 0
            ? ImVec4(0.6f, 1.f, 0.6f, 1.f)
            : ImVec4(1.f, 0.8f, 0.2f, 1.f);

        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Text("  %-16llu %-10s %-5d  %s",
            uid,
            typeName(res->type),
            res->referenceCount,
            path.empty() ? "(unknown)" : path.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::End();
}