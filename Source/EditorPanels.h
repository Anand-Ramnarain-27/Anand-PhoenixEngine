#pragma once
#include "EditorPanel.h"
#include "ResourceCommon.h"
#include "CollisionTypes.h"
#include <vector>
#include <string>

enum class LogTag { Info, Ok, Warn, Error };

struct ConsoleEntry {
    std::string text;    // full original string
    std::string message; // text after the [tag] prefix
    ImVec4      color;
    LogTag      tag = LogTag::Info;
};

class ConsolePanel : public EditorPanel {
public:
    explicit ConsolePanel(ModuleEditor* editor) : EditorPanel(editor) {}
    const char* getName() const override { return "Console"; }

    void add(const char* text, const ImVec4& color = EditorColors::White);
    void clear() { m_entries.clear(); }

protected:
    void drawContent() override;

private:
    static LogTag detectTag(const char* text, std::string& msgOut);

    std::vector<ConsoleEntry> m_entries;
    bool  m_autoScroll = true;
    char  m_filterBuf[128] = {};
    char  m_cmdBuf[256]    = {};
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

// Shows live collision pipeline statistics (broad/mid/narrow pair counts and
// individual contact details) updated every frame by CollisionSystem::run().
class CollisionDebugPanel : public EditorPanel {
public:
    explicit CollisionDebugPanel(ModuleEditor* editor)
        : EditorPanel(editor) { open = false; }
    const char* getName() const override { return "Collision Debug"; }

protected:
    void drawContent() override;
};
