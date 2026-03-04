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
    case ResourceBase::Type::Mesh:     return "Mesh";
    case ResourceBase::Type::Texture:  return "Texture";
    case ResourceBase::Type::Material: return "Material";
    case ResourceBase::Type::Model:    return "Model";
    case ResourceBase::Type::Scene:    return "Scene";
    default:                           return "Unknown";
    }
}

void ResourcesPanel::draw()
{
    ImGui::Begin("Resources", &open);

    const auto& resources = app->getResources()->getLoadedResources();

    ImGui::Text("Resources in memory: %d", (int)resources.size());
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.f));
    ImGui::Text("  %-10s  %-5s  %s", "Type", "Refs", "Asset Path");
    ImGui::PopStyleColor();
    ImGui::Separator();

    for (auto& [uid, res] : resources)
    {
        std::string path = app->getAssets()->getPathFromUID(uid);
        if (path.empty())
            path = app->getResources()->getLibraryPath(uid);
        if (path.empty())
            path = "(uid=" + std::to_string(uid) + ")";

        ImVec4 col;
        switch (res->type)
        {
        case ResourceBase::Type::Mesh:     col = ImVec4(0.6f, 0.9f, 1.0f, 1.f); break;
        case ResourceBase::Type::Material: col = ImVec4(1.0f, 0.85f, 0.5f, 1.f); break;
        case ResourceBase::Type::Texture:  col = ImVec4(0.8f, 0.6f, 1.0f, 1.f); break;
        default:                           col = ImVec4(0.6f, 1.f, 0.6f, 1.f); break;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Text("  %-10s  %-5d  %s",
            typeName(res->type),
            res->referenceCount,
            path.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::End();
}