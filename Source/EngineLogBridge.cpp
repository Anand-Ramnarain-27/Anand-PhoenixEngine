// The editor's PhoenixEngineLogToConsole(): HotReloadManager hands it to script DLLs, and it forwards their log
// lines to the Console panel. Player links PlayerLogBridge.cpp instead, as it has no editor.
#include "Globals.h"
#include "Application.h"
#include "ModuleEditor.h"

void PhoenixEngineLogToConsole(const char* text, float r, float g, float b, float a){
    if (app && app->getEditor())
        app->getEditor()->log(text, ImVec4(r, g, b, a));
}
