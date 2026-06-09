#include "Globals.h"
#include "SceneManager.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "IScene.h"
#include "ModuleScene.h"
#include "GameObject.h"
#include "ComponentMesh.h"
#include "ComponentAnimation.h"
#include "SceneSerializer.h"

SceneManager::~SceneManager() { clearScene(); }

void SceneManager::setScene(std::unique_ptr<IScene> scene, ID3D12Device* device){
    if (m_editingPrefab) exitPrefabEdit();
    clearScene();
    if (scene && scene->initialize(device)) { activeScene = std::move(scene); activeScene->onEnter(); }
}

void SceneManager::clearScene(){
    if (m_editingPrefab) exitPrefabEdit();
    if (activeScene) {
        // Make sure the GPU is done with any in-flight command lists before we start
        // destroying GameObjects/Components — meshes own GPU resources (e.g. the legacy
        // vertex buffer "MeshVB_Legacy") that would otherwise be deleted while still
        // referenced by a command list, triggering OBJECT_DELETED_WHILE_STILL_IN_USE.
        if (auto* d3d = app->getD3D12()) d3d->flush();
        activeScene->onExit(); activeScene->shutdown(); activeScene.reset();
    }
    state = PlayState::Stopped;
    hasSerializedState = false;
}

ModuleScene* SceneManager::getModuleScene() const{
    if (m_editingPrefab && m_prefabScene) return m_prefabScene;
    return activeScene ? activeScene->getModuleScene() : nullptr;
}

void SceneManager::play(){
    if (!activeScene || m_editingPrefab) return;
    if (state == PlayState::Stopped) {
        if (auto* ms = activeScene->getModuleScene()) hasSerializedState = SceneSerializer::SaveTempScene(ms);
    }
    state = PlayState::Playing;
}

void SceneManager::pause(){
    if (state == PlayState::Playing) state = PlayState::Paused;
    else if (state == PlayState::Paused) state = PlayState::Playing;
}

void SceneManager::stop(){
    if (!activeScene || state == PlayState::Stopped) return;

    // Entering Stop tears down and rebuilds the scene graph (LoadTempScene / reset),
    // which destroys GameObjects/Components — including ComponentMesh, whose Mesh
    // resources (legacy VB/IB committed buffers) may still be referenced by command
    // lists the GPU hasn't finished executing yet (this frame's render, or the previous
    // frame still in flight). Destroying them early causes the D3D12 debug layer to
    // raise EXECUTION ERROR #921 OBJECT_DELETED_WHILE_STILL_IN_USE, which — with
    // "break on message" enabled — surfaces as an unhandled exception/crash.
    // Block here until the GPU has fully drained before tearing anything down.
    if (auto* d3d = app->getD3D12()) d3d->flush();

    auto* ms = activeScene->getModuleScene();
    if (hasSerializedState && ms) {
        if (!SceneSerializer::LoadTempScene(ms)) { LOG("SceneManager: Failed to restore temp scene, falling back to reset()"); activeScene->reset(); }
        hasSerializedState = false;
    }
    else activeScene->reset();
    state = PlayState::Stopped;
}

void SceneManager::update(float deltaTime){
    if (m_editingPrefab) return;
    if (activeScene && state == PlayState::Playing) activeScene->update(deltaTime);
}

void SceneManager::updateAnimations(float deltaTime){
    if (m_editingPrefab) return;
    if (state == PlayState::Playing) return; // already updated via update()
    auto* ms = getModuleScene();
    if (!ms) return;
    std::function<void(GameObject*)> visit = [&](GameObject* go) {
        if (!go || !go->isActive()) return;
        if (auto* anim = go->getComponent<ComponentAnimation>()) anim->update(deltaTime);
        for (auto* child : go->getChildren()) visit(child);
    };
    visit(ms->getRoot());
}

static void renderModuleScene(ModuleScene* ms, ID3D12GraphicsCommandList* cmd){
    if (!ms) return;
    std::function<void(GameObject*)> visit = [&](GameObject* node) {
        if (!node || !node->isActive()) return;
        if (auto* mesh = node->getComponent<ComponentMesh>()) mesh->render(cmd);
        for (auto* child : node->getChildren()) visit(child);
        };
    visit(ms->getRoot());
}

void SceneManager::render(ID3D12GraphicsCommandList* cmd, const ModuleCamera& camera, uint32_t w, uint32_t h){
    if (m_editingPrefab) {
        renderModuleScene(m_prefabScene, cmd);
        return;
    }
    if (activeScene) activeScene->render(cmd, camera, w, h);
}

void SceneManager::onViewportResized(uint32_t w, uint32_t h){
    if (activeScene) activeScene->onViewportResized(w, h);
}

bool SceneManager::saveCurrentScene(const std::string& filePath){
    if (m_editingPrefab) { LOG("SceneManager: Cannot save scene while editing a prefab"); return false; }
    auto* ms = activeScene ? activeScene->getModuleScene() : nullptr;
    if (!ms) { LOG("SceneManager: No active scene to save"); return false; }
    return SceneSerializer::SaveScene(ms, filePath);
}

bool SceneManager::loadScene(const std::string& filePath){
    if (m_editingPrefab) { LOG("SceneManager: Cannot load scene while editing a prefab"); return false; }
    auto* ms = activeScene ? activeScene->getModuleScene() : nullptr;
    if (!ms) { LOG("SceneManager: No active scene to load into"); return false; }
    app->getD3D12()->flush();
    return SceneSerializer::LoadScene(filePath, ms);
}

void SceneManager::enterPrefabEdit(ModuleScene* prefabScene, const std::string& prefabName){
    if (m_editingPrefab) exitPrefabEdit();
    m_savedScene = activeScene ? activeScene->getModuleScene() : nullptr;
    m_prefabScene = prefabScene;
    m_prefabEditName = prefabName;
    m_editingPrefab = true;
}

void SceneManager::exitPrefabEdit(){
    if (!m_editingPrefab) return;
    m_editingPrefab = false;
    m_prefabEditName.clear();
    m_prefabScene = nullptr;
    m_savedScene = nullptr;
}
