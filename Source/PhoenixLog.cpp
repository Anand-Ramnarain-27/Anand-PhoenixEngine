#include "Globals.h"
#include "PhoenixLog.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <mutex>

namespace PhoenixLog {
namespace {

constexpr int kCategoryCount = static_cast<int>(LogCategory::Count);

// Each category's threshold, stored as an offset from kDefaultThreshold so the zero-initialised array is already
// correct before any static constructor runs (other translation units may log during static initialisation).
// Read by every PHX_LOG on any thread; written only from the Console panel on the main thread.
std::atomic<int> g_thresholdOffsets[kCategoryCount];

std::mutex g_scriptMutex;
std::map<std::string, LogLevel> g_scriptThresholds;

std::mutex g_consoleMutex;
ConsoleSink g_consoleSink = nullptr;
std::vector<std::pair<LogLevel, std::string>> g_consoleQueue;

std::string formatV(const char* format, va_list args){
    va_list copy;
    va_copy(copy, args);
    const int len = _vscprintf(format, copy);
    va_end(copy);
    if (len < 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    vsnprintf(out.data(), out.size() + 1, format, args);
    return out;
}

} // namespace

const char* categoryName(LogCategory category){
    static const char* const kNames[kCategoryCount] = {
        "Core", "Render", "Assets", "Scene", "Script", "Editor", "Physics", "AI", "UI"
    };
    const int i = static_cast<int>(category);
    return (i >= 0 && i < kCategoryCount) ? kNames[i] : "?";
}

const char* levelName(LogLevel level){
    switch (level){
    case LogLevel::Error: return "Error";
    case LogLevel::Warning: return "Warning";
    case LogLevel::Info: return "Info";
    case LogLevel::Verbose: return "Verbose";
    }
    return "?";
}

bool isEnabled(LogCategory category, LogLevel level){
    const int i = static_cast<int>(category);
    if (i < 0 || i >= kCategoryCount) return true;
    return static_cast<int>(level) <= static_cast<int>(threshold(category));
}

LogLevel threshold(LogCategory category){
    const int i = static_cast<int>(category);
    if (i < 0 || i >= kCategoryCount) return kDefaultThreshold;
    return static_cast<LogLevel>(static_cast<int>(kDefaultThreshold) + g_thresholdOffsets[i].load(std::memory_order_relaxed));
}

void setThreshold(LogCategory category, LogLevel level){
    const int i = static_cast<int>(category);
    if (i >= 0 && i < kCategoryCount)
        g_thresholdOffsets[i].store(static_cast<int>(level) - static_cast<int>(kDefaultThreshold), std::memory_order_relaxed);
}

void write(LogCategory category, LogLevel level, const char* file, int line, const char* format, ...){
    va_list args;
    va_start(args, format);
    const std::string message = formatV(format, args);
    va_end(args);

    std::string tagged = std::string("[") + categoryName(category) + "] ";
    if (level == LogLevel::Error) tagged += "ERROR: ";
    else if (level == LogLevel::Warning) tagged += "WARNING: ";
    tagged += message;

    char location[64];
    snprintf(location, sizeof(location), "(%d) : ", line);
    OutputDebugStringA(("\n" + std::string(file) + location + tagged).c_str());

    std::lock_guard<std::mutex> lock(g_consoleMutex);
    if (g_consoleSink) g_consoleQueue.emplace_back(level, std::move(tagged));
}

bool isScriptEnabled(const char* category, int level){
    if (!category) return true;
    std::lock_guard<std::mutex> lock(g_scriptMutex);
    auto it = g_scriptThresholds.try_emplace(category, kDefaultThreshold).first;
    return level <= static_cast<int>(it->second);
}

std::vector<ScriptCategory> scriptCategories(){
    std::lock_guard<std::mutex> lock(g_scriptMutex);
    std::vector<ScriptCategory> out;
    out.reserve(g_scriptThresholds.size());
    for (const auto& [name, t] : g_scriptThresholds) out.push_back({ name, t });
    return out;
}

void setScriptThreshold(const std::string& category, LogLevel level){
    std::lock_guard<std::mutex> lock(g_scriptMutex);
    g_scriptThresholds[category] = level;
}

void setConsoleSink(ConsoleSink sink){
    std::lock_guard<std::mutex> lock(g_consoleMutex);
    g_consoleSink = sink;
    if (!sink) g_consoleQueue.clear();
}

void drainConsole(){
    std::vector<std::pair<LogLevel, std::string>> pending;
    ConsoleSink sink;
    {
        std::lock_guard<std::mutex> lock(g_consoleMutex);
        pending.swap(g_consoleQueue);
        sink = g_consoleSink;
    }
    if (!sink) return;
    for (const auto& [level, text] : pending) sink(level, text.c_str());
}

} // namespace PhoenixLog

// Hands script DLLs their category filter; HotReloadManager passes this to SetPhoenixEngineLogFilterFn.
bool PhoenixEngineLogEnabled(const char* category, int level){
    return PhoenixLog::isScriptEnabled(category, level);
}
