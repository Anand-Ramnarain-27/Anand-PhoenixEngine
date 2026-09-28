// SceneManager::getModuleScene() and the script-facing request queue - split
// out of SceneManager.cpp so they can be linked into GameScript.dll (via
// PhoenixCore) without pulling in ComponentMesh/ComponentAnimation/
// SceneSerializer/ModuleD3D12, which SceneManager.cpp's other methods
// (render/updateAnimations/play/loadScene/processRuntimeRequests) need but
// these don't.
#include "Globals.h"
#include "SceneManager.h"
#include "IScene.h"
#include "SceneGraph.h"

SceneGraph* SceneManager::getModuleScene() const{
    if (m_editingPrefab && m_prefabScene) return m_prefabScene;
    return activeScene ? activeScene->getModuleScene() : nullptr;
}

void SceneManager::requestSceneLoad(const std::string& filePath){
    if (!m_pendingScenePath.empty() && m_pendingScenePath != filePath)
        LOG("SceneManager: scene load '%s' replaces pending '%s'", filePath.c_str(), m_pendingScenePath.c_str());
    m_pendingScenePath = filePath;
}

void SceneManager::requestPrefabSpawn(const std::string& prefabName, const Vector3& position, const Quaternion& rotation){
    m_pendingSpawns.push_back({ prefabName, position, rotation });
}
