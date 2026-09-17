// RuntimeCore::getActiveModuleScene() - split out of RuntimeCore.cpp so it
// can be linked into GameScript.dll (via PhoenixCore) without the renderer.
// RuntimeCore.cpp's other methods (tick/render/renderSceneWithCamera/init/
// cleanUp) reference nearly every render pass in the engine; this accessor
// touches only m_sceneManager, which GameScript.dll never constructs itself
// (only calls a method on an already-running instance via app->getRuntimeCore()).
#include "Globals.h"
#include "RuntimeCore.h"
#include "SceneGraph.h"
#include "SceneManager.h"

SceneGraph* RuntimeCore::getActiveModuleScene() const{
    return m_sceneManager ? m_sceneManager->getModuleScene() : nullptr;
}
