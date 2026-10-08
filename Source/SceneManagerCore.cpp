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
    return m_activeScene ? m_activeScene->getModuleScene() : nullptr;
}

void SceneManager::requestSceneLoad(const std::string& filePath){
    if (!m_pendingScenePath.empty() && m_pendingScenePath != filePath)
        PHX_LOG(Scene, Info, "SceneManager: scene load '%s' replaces pending '%s'", filePath.c_str(), m_pendingScenePath.c_str());
    m_pendingScenePath = filePath;
}

void SceneManager::requestPrefabSpawn(const std::string& prefabName, const Vector3& position, const Quaternion& rotation){
    m_pendingSpawns.push_back({ prefabName, position, rotation });
}

bool SceneManager::requestTransition(const std::string& filePath){
    if (isTransitionActive()){
        PHX_LOG(Scene, Warning, "SceneManager: transition to '%s' ignored - another transition is running", filePath.c_str());
        return false;
    }
    if (m_editingPrefab){
        PHX_LOG(Scene, Warning, "SceneManager: transition to '%s' ignored while editing a prefab", filePath.c_str());
        return false;
    }
    m_transition.requestedPath = filePath;
    m_transition.readySignalled = false;
    return true;
}

void SceneManager::notifyTransitionReady(){
    if (m_transition.phase == SceneTransitionState::Phase::WarmUp) m_transition.readySignalled = true;
}
