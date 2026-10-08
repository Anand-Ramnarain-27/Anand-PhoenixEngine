#pragma once
// Categorised, levelled logging. Every message goes to the debugger output (OutputDebugString) and, in the editor,
// to the Console panel, if its category's threshold lets it through.

#include <string>
#include <vector>

/// How important a message is. A category shows messages at or above its threshold (lower value = more important).
enum class LogLevel : int { Error = 0, Warning = 1, Info = 2, Verbose = 3 };

/// Engine subsystems that log. Script DLLs use named categories instead (see PhoenixLog::isScriptEnabled).
enum class LogCategory : int { Core, Render, Assets, Scene, Script, Editor, Physics, AI, UI, Count };

namespace PhoenixLog {

/// Threshold every category starts at: Error, Warning and Info shown, Verbose hidden.
constexpr LogLevel kDefaultThreshold = LogLevel::Info;

const char* categoryName(LogCategory category);
const char* levelName(LogLevel level);

bool isEnabled(LogCategory category, LogLevel level);
LogLevel threshold(LogCategory category);
void setThreshold(LogCategory category, LogLevel level);

/// Formats and emits one message. Use PHX_LOG, which skips the formatting when the message is filtered out.
void write(LogCategory category, LogLevel level, const char* file, int line, const char* format, ...);

/// Script DLL categories ("Enemy", "VFX", ...) are registered by name the first time they are queried, so the
/// Console panel can list them. The DLL asks through the function handed over by HotReloadManager.
bool isScriptEnabled(const char* category, int level);

struct ScriptCategory {
    std::string name;
    LogLevel threshold;
};
std::vector<ScriptCategory> scriptCategories();
void setScriptThreshold(const std::string& category, LogLevel level);

/// Console sink. Messages can come from asset-import worker threads, so they are queued here and handed to the
/// sink on the main thread by drainConsole(). Nothing is queued until a sink is set (the Player never sets one).
using ConsoleSink = void (*)(LogLevel level, const char* text);
void setConsoleSink(ConsoleSink sink);
void drainConsole();

} // namespace PhoenixLog

/// Logs `format` (printf-style) under `category` (a LogCategory name) at `level` (a LogLevel name), e.g.
/// PHX_LOG(Render, Warning, "TrailPass: %u segments dropped", n);
#define PHX_LOG(category, level, format, ...)                                                                   \
    do {                                                                                                        \
        if (PhoenixLog::isEnabled(LogCategory::category, LogLevel::level))                                      \
            PhoenixLog::write(LogCategory::category, LogLevel::level, __FILE__, __LINE__, format, __VA_ARGS__); \
    } while (0)
