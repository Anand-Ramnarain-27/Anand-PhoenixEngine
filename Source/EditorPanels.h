#pragma once
#include "EditorPanel.h"
#include "ResourceCommon.h"
#include <vector>
#include <string>

struct ConsoleEntry { std::string text; ImVec4 color; };

class ConsolePanel : public EditorPanel {
public:
    explicit ConsolePanel(ModuleEditor* editor) : EditorPanel(editor) {}
    const char* getName() const override { return "Console"; }

    void add(const char* text, const ImVec4& color = EditorColors::White) { m_entries.push_back({ text, color }); }
    void clear() { m_entries.clear(); }

protected:
    void drawContent() override {
        if (ImGui::Button("Clear")) clear();
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll);
        ImGui::Separator();
        ImGui::BeginChild("##scroll");
        for (const auto& e : m_entries) {
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

class PerformancePanel : public EditorPanel {
public:
    explicit PerformancePanel(ModuleEditor* editor) : EditorPanel(editor) { open = false; }
    const char* getName() const override { return "Performance"; }

    void pushFPS(float fps) { m_fpsHistory[m_fpsIdx] = fps; m_fpsIdx = (m_fpsIdx + 1) % kHistory; }
    void setGpuMs(double ms) { m_gpuMs = ms; m_gpuReady = true; }
    void setMemory(uint64_t gpu, uint64_t ram) { m_gpuMem = gpu; m_ramMem = ram; }

protected:
    void drawContent() override;

private:
    static constexpr int kHistory = 200;
    float m_fpsHistory[kHistory] = {};
    int m_fpsIdx = 0;
    double m_gpuMs = 0.0;
    bool m_gpuReady = false;
    uint64_t m_gpuMem = 0;
    uint64_t m_ramMem = 0;
};

class ResourcesPanel : public EditorPanel {
public:
    explicit ResourcesPanel(ModuleEditor* editor) : EditorPanel(editor) {}
    const char* getName() const override { return "Resources"; }

protected:
    void drawContent() override;

private:
    static ImVec4 typeColor(ResourceBase::Type t);
    static const char* typeName(ResourceBase::Type t);
};