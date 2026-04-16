#include "Globals.h"
#include "SceneManager.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "IScene.h"
#include "ModuleScene.h"
#include "GameObject.h"
#include "ComponentMesh.h"
#include "SceneSerializer.h"

SceneManager::~SceneManager() { clearScene(); }

void SceneManager::setScene(std::unique_ptr<IScene> scene, ID3D12Device* device) {
    if (m_editingPrefab) exitPrefabEdit();
    clearScene();
    if (scene && scene->initialize(device)) { activeScene = std::move(scene); activeScene->onEnter(); }
}

void SceneManager::clearScene() {
    if (m_editingPrefab) exitPrefabEdit();
    if (activeScene) { activeScene->onExit(); activeScene->shutdown(); activeScene.reset(); }
    state = PlayState::Stopped;
    hasSerializedState = false;
}

ModuleScene* SceneManager::getModuleScene() const {
    if (m_editingPrefab && m_prefabScene) return m_prefabScene;
    return activeScene ? activeScene->getModuleScene() : nullptr;
}

void SceneManager::play() {
    if (!activeScene || m_editingPrefab) return;
    if (state == PlayState::Stopped) {
        if (auto* ms = activeScene->getModuleScene()) hasSerializedState = SceneSerializer::SaveTempScene(ms);
    }
    state = PlayState::Playing;
}

void SceneManager::pause() {
    if (state == PlayState::Playing) state = PlayState::Paused;
    else if (state == PlayState::Paused) state = PlayState::Playing;
}

void SceneManager::stop() {
    if (!activeScene || state == PlayState::Stopped) return;
    auto* ms = activeScene->getModuleScene();
    if (hasSerializedState && ms) {
        if (!SceneSerializer::LoadTempScene(ms)) { LOG("SceneManager: Failed to restore temp scene, falling back to reset()"); activeScene->reset(); }
        hasSerializedState = false;
    }
    else activeScene->reset();
    state = PlayState::Stopped;
}

void SceneManager::update(float deltaTime) {
    if (m_editingPrefab) return;
    if (activeScene && state == PlayState::Playing) activeScene->update(deltaTime);
}

static void renderModuleScene(ModuleScene* ms, ID3D12GraphicsCommandList* cmd) {
    if (!ms) return;
    std::function<void(GameObject*)> visit = [&](GameObject* node) {
        if (!node || !node->isActive()) return;
        if (auto* mesh = node->getComponent<ComponentMesh>()) mesh->render(cmd);
        for (auto* child : node->getChildren()) visit(child);
        };
    visit(ms->getRoot());
}

void SceneManager::render(ID3D12GraphicsCommandList* cmd, const ModuleCamera& camera, uint32_t w, uint32_t h) {
    if (m_editingPrefab) {
        renderModuleScene(m_prefabScene, cmd);
        return;
    }
    if (activeScene) activeScene->render(cmd, camera, w, h);
}

void SceneManager::onViewportResized(uint32_t w, uint32_t h) {
    if (activeScene) activeScene->onViewportResized(w, h);
}

bool SceneManager::saveCurrentScene(const std::string& filePath) {
    if (m_editingPrefab) { LOG("SceneManager: Cannot save scene while editing a prefab"); return false; }
    auto* ms = activeScene ? activeScene->getModuleScene() : nullptr;
    if (!ms) { LOG("SceneManager: No active scene to save"); return false; }
    return SceneSerializer::SaveScene(ms, filePath);
}

bool SceneManager::loadScene(const std::string& filePath) {
    if (m_editingPrefab) { LOG("SceneManager: Cannot load scene while editing a prefab"); return false; }
    auto* ms = activeScene ? activeScene->getModuleScene() : nullptr;
    if (!ms) { LOG("SceneManager: No active scene to load into"); return false; }
    app->getD3D12()->flush();
    return SceneSerializer::LoadScene(filePath, ms);
}

void SceneManager::enterPrefabEdit(ModuleScene* prefabScene, const std::string& prefabName) {
    if (m_editingPrefab) exitPrefabEdit();
    m_savedScene = activeScene ? activeScene->getModuleScene() : nullptr;
    m_prefabScene = prefabScene;
    m_prefabEditName = prefabName;
    m_editingPrefab = true;
}

void SceneManager::exitPrefabEdit() {
    if (!m_editingPrefab) return;
    m_editingPrefab = false;
    m_prefabEditName.clear();
    m_prefabScene = nullptr;
    m_savedScene = nullptr;
}