// Real implementation of PhoenixEngineLogToConsole() (declared in
// HotReloadManager.cpp, handed across the DLL boundary to script DLLs the
// same way Application* app is via SetPhoenixEngineApp) - forwards to the
// editor's visible Console panel. Engine.vcxproj-only: ModuleEditor doesn't
// exist in Player.vcxproj's build (no editor UI at runtime) - see
// PlayerLogBridge.cpp for the no-op stub Player links instead.
#include "Globals.h"
#include "Application.h"
#include "ModuleEditor.h"

void PhoenixEngineLogToConsole(const char* text, float r, float g, float b, float a){
    if (app && app->getEditor())
        app->getEditor()->log(text, ImVec4(r, g, b, a));
}
