// GameScript.dll's own `app`. A DLL doesn't share globals with the exe that loads it, so the PhoenixCore.lib code
// linked in here would see null; HotReloadManager calls SetPhoenixEngineApp() right after loading the DLL to
// hand over the engine's Application. Engine APIs must not be called before that.
#include "Application.h"
#include "ScriptExport.h"

Application* app = nullptr;

extern "C" SCRIPT_API void SetPhoenixEngineApp(Application* engineApp){
    app = engineApp;
}

// Same handoff for the callback that forwards script log lines to the editor's Console (EngineLogBridge.cpp).
using EngineLogFn = void(*)(const char*, float, float, float, float);
EngineLogFn g_engineLogFn = nullptr;

extern "C" SCRIPT_API void SetPhoenixEngineLogFn(EngineLogFn fn){
    g_engineLogFn = fn;
}
