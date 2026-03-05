#pragma once
#include "EditorPanel.h"
#include "Application.h"
#include "ModuleResources.h"
#include "ModuleAssets.h"
#include "ResourceCommon.h"
#include <vector>
#include <string>

struct ConsoleEntry { std::string text; ImVec4 color; };

class ConsolePanel : public EditorPanel
{
public:
    explicit ConsolePanel(ModuleEditor* editor) : EditorPanel(editor) {}
    const char* getName() const override { return "Console"; }

    void add(const char* text, const ImVec4& color = EditorColors::White) { m_entries.push_back({ text, color }); }
    void clear() { m_entries.clear(); }

protected:
    void drawContent() override
    {
        if (ImGui::Button("Clear")) clear();
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll);
        ImGui::Separator();
        ImGui::BeginChild("##scroll");
        for (const auto& e : m_entries)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, e.color);
            ImGui::TextUnformatted(e.text.c_str());
            ImGui::PopStyleColor();
        }
        if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }

private:
    std::vector<ConsoleEntry> m_entries;
    bool m_autoScroll = true;
};

class PerformancePanel : public EditorPanel
{
public:
    explicit PerformancePanel(ModuleEditor* editor) : EditorPanel(editor) { open = false; }
    const char* getName() const override { return "Performance"; }

    void pushFPS(float fps) { m_fpsHistory[m_fpsIdx] = fps; m_fpsIdx = (m_fpsIdx + 1) % kHistory; }
    void setGpuMs(double ms) { m_gpuMs = ms; m_gpuReady = true; }
    void setMemory(uint64_t gpu, uint64_t ram) { m_gpuMem = gpu; m_ramMem = ram; }

protected:
    void drawContent() override
    {
        ImGui::Text("FPS:  %.1f", app->getFPS());
        ImGui::Text("CPU:  %.2f ms", app->getAvgElapsedMs());
        if (m_gpuReady) ImGui::Text("GPU:  %.2f ms", m_gpuMs);
        ImGui::Separator();
        ImGui::Text("VRAM: %llu MB", m_gpuMem);
        ImGui::Text("RAM:  %llu MB", m_ramMem);
        ImGui::Separator();

        float ordered[kHistory];
        float maxFPS = 60.0f;
        for (int i = 0; i < kHistory; ++i)
        {
            ordered[i] = m_fpsHistory[(m_fpsIdx + i) % kHistory];
            if (ordered[i] > maxFPS) maxFPS = ordered[i];
        }
        ImGui::PlotLines("##fps", ordered, kHistory, 0, nullptr, 0, maxFPS * 1.1f, ImVec2(-1, 80));
    }

private:
    static constexpr int kHistory = 200;
    float    m_fpsHistory[kHistory] = {};
    int      m_fpsIdx = 0;
    double   m_gpuMs = 0.0;
    bool     m_gpuReady = false;
    uint64_t m_gpuMem = 0;
    uint64_t m_ramMem = 0;
};

class ResourcesPanel : public EditorPanel
{
public:
    explicit ResourcesPanel(ModuleEditor* editor) : EditorPanel(editor) {}
    const char* getName() const override { return "Resources"; }

protected:
    void drawContent() override
    {
        const auto& resources = app->getResources()->getLoadedResources();
        ImGui::Text("Resources in memory: %d", (int)resources.size());
        ImGui::Separator();
        textMuted("  %-10s  %-5s  %s", "Type", "Refs", "Asset Path");
        ImGui::Separator();

        for (auto& [uid, res] : resources)
        {
            std::string path = app->getAssets()->getPathFromUID(uid);
            if (path.empty()) path = app->getResources()->getLibraryPath(uid);
            if (path.empty()) path = "(uid=" + std::to_string(uid) + ")";
            ImGui::PushStyleColor(ImGuiCol_Text, typeColor(res->type));
            ImGui::Text("  %-10s  %-5d  %s", typeName(res->type), res->referenceCount, path.c_str());
            ImGui::PopStyleColor();
        }
    }

private:
    static ImVec4 typeColor(ResourceBase::Type t)
    {
        switch (t)
        {
        case ResourceBase::Type::Mesh:     return { 0.6f, 0.9f, 1.0f, 1.f };
        case ResourceBase::Type::Material: return { 1.0f, 0.85f, 0.5f, 1.f };
        case ResourceBase::Type::Texture:  return { 0.8f, 0.6f, 1.0f, 1.f };
        default:                           return { 0.6f, 1.f,  0.6f, 1.f };
        }
    }

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
};