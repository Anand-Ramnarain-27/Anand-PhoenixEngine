// GameScript.dll's copy of the engine's global Application* app (declared
// `extern Application* app;` in Application.h, defined once per binary -
// PhoenixEngine.cpp/PlayerMain.cpp define the engine process's own copy).
// A DLL does NOT share global/static storage with the EXE that loaded it -
// this is a SEPARATE symbol at a separate address, so PhoenixCore.lib code
// linked into this DLL (Phoenix::Input today) would see nullptr here forever
// unless the host process hands its own `app` pointer across the DLL
// boundary explicitly. HotReloadManager::loadLibraryInternal() does that:
// right after LoadLibraryA succeeds, it looks up and calls
// SetPhoenixEngineApp() below with its own `app`. Until that call happens
// (or if this DLL is loaded by something that doesn't make it), this stays
// nullptr and anything dereferencing it will crash - same failure class as
// calling engine APIs too early already is in Engine/Player.
#include "Application.h"
#include "ScriptExport.h"

Application* app = nullptr;

extern "C" SCRIPT_API void SetPhoenixEngineApp(Application* engineApp){
    app = engineApp;
}

// Same handoff, for a callback that forwards script log calls into the
// editor's visible Console panel instead of just OutputDebugStringA. See
// Anand-PhoenixEngine's HotReloadManager.cpp/EngineLogBridge.cpp.
using EngineLogFn = void(*)(const char*, float, float, float, float);
EngineLogFn g_engineLogFn = nullptr;

extern "C" SCRIPT_API void SetPhoenixEngineLogFn(EngineLogFn fn){
    g_engineLogFn = fn;
}
