// SceneManager::getModuleScene() - split out of SceneManager.cpp so it can
// be linked into GameScript.dll (via PhoenixCore) without pulling in
// ComponentMesh/ComponentAnimation/SceneSerializer/ModuleD3D12, which
// SceneManager.cpp's other methods (render/updateAnimations/play/loadScene)
// need but this accessor doesn't.
#include "Globals.h"
#include "SceneManager.h"
#include "IScene.h"
#include "SceneGraph.h"

SceneGraph* SceneManager::getModuleScene() const{
    if (m_editingPrefab && m_prefabScene) return m_prefabScene;
    return activeScene ? activeScene->getModuleScene() : nullptr;
}
