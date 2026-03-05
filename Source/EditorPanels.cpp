#include "Globals.h"
#include "EditorPanels.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleAssets.h"
#include "ResourceCommon.h"

void PerformancePanel::drawContent() {
    ImGui::Text("FPS:  %.1f", app->getFPS());
    ImGui::Text("CPU:  %.2f ms", app->getAvgElapsedMs());
    if (m_gpuReady) ImGui::Text("GPU:  %.2f ms", m_gpuMs);
    ImGui::Separator();
    ImGui::Text("VRAM: %llu MB", m_gpuMem);
    ImGui::Text("RAM:  %llu MB", m_ramMem);
    ImGui::Separator();
    float ordered[kHistory];
    float maxFPS = 60.0f;
    for (int i = 0; i < kHistory; ++i) {
        ordered[i] = m_fpsHistory[(m_fpsIdx + i) % kHistory];
        if (ordered[i] > maxFPS) maxFPS = ordered[i];
    }
    ImGui::PlotLines("##fps", ordered, kHistory, 0, nullptr, 0, maxFPS * 1.1f, ImVec2(-1, 80));
}

void ResourcesPanel::drawContent() {
    const auto& resources = app->getResources()->getLoadedResources();
    ImGui::Text("Resources in memory: %d", (int)resources.size());
    ImGui::Separator();
    textMuted("  %-10s  %-5s  %s", "Type", "Refs", "Asset Path");
    ImGui::Separator();
    for (auto& [uid, res] : resources) {
        std::string path = app->getAssets()->getPathFromUID(uid);
        if (path.empty()) path = app->getResources()->getLibraryPath(uid);
        if (path.empty()) path = "(uid=" + std::to_string(uid) + ")";
        ImGui::PushStyleColor(ImGuiCol_Text, typeColor(res->type));
        ImGui::Text("  %-10s  %-5d  %s", typeName(res->type), res->referenceCount, path.c_str());
        ImGui::PopStyleColor();
    }
}

ImVec4 ResourcesPanel::typeColor(ResourceBase::Type t) {
    switch (t) {
    case ResourceBase::Type::Mesh:     return { 0.6f, 0.9f, 1.0f, 1.f };
    case ResourceBase::Type::Material: return { 1.0f, 0.85f, 0.5f, 1.f };
    case ResourceBase::Type::Texture:  return { 0.8f, 0.6f, 1.0f, 1.f };
    default:                           return { 0.6f, 1.f, 0.6f, 1.f };
    }
}

const char* ResourcesPanel::typeName(ResourceBase::Type t) {
    switch (t) {
    case ResourceBase::Type::Mesh:     return "Mesh";
    case ResourceBase::Type::Texture:  return "Texture";
    case ResourceBase::Type::Material: return "Material";
    case ResourceBase::Type::Model:    return "Model";
    case ResourceBase::Type::Scene:    return "Scene";
    default:                           return "Unknown";
    }
}