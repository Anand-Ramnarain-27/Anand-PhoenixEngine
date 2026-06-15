#include "Globals.h"
#include "ResourcesPanel.h"
#include "EditorColors.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleAssets.h"
#include "ResourceCommon.h"

void ResourcesPanel::drawContent(){
    const auto& resources = app->getResources()->getLoadedResources();
    ImGui::Text("Resources in memory: %d", (int)resources.size());
    ImGui::Separator();
    textMuted("  %-10s  %-5s  %s", "Type", "Refs", "Asset Path");
    ImGui::Separator();
    for (auto& [uid, res] : resources){
        std::string path = app->getAssets()->getPathFromUID(uid);
        if (path.empty()) path = app->getResources()->getLibraryPath(uid);
        if (path.empty()) path = "(uid=" + std::to_string(uid) + ")";
        ImGui::PushStyleColor(ImGuiCol_Text, typeColor(res->type));
        ImGui::Text("  %-10s  %-5d  %s", typeName(res->type), res->referenceCount, path.c_str());
        ImGui::PopStyleColor();
    }
}

ImVec4 ResourcesPanel::typeColor(ResourceBase::Type t){
    switch (t){
    case ResourceBase::Type::Mesh: return { 0.6f, 0.9f, 1.0f, 1.f };
    case ResourceBase::Type::Material: return { 1.0f, 0.85f, 0.5f, 1.f };
    case ResourceBase::Type::Texture: return { 0.8f, 0.6f, 1.0f, 1.f };
    default: return { 0.6f, 1.f, 0.6f, 1.f };
    }
}

const char* ResourcesPanel::typeName(ResourceBase::Type t){
    switch (t){
    case ResourceBase::Type::Mesh: return "Mesh";
    case ResourceBase::Type::Texture: return "Texture";
    case ResourceBase::Type::Material: return "Material";
    case ResourceBase::Type::Model: return "Model";
    case ResourceBase::Type::Scene: return "Scene";
    default: return "Unknown";
    }
}
