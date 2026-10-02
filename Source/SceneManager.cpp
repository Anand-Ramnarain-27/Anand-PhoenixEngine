#include "Globals.h"
#include "SceneManager.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "IScene.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "ComponentMesh.h"
#include "ComponentAnimation.h"
#include "SceneSerializer.h"
#include "BuildSettings.h"
#include "ModuleFileSystem.h"
#include "ModuleCamera.h"
#include "ComponentTransform.h"
#include "PrefabManager.h"
#include "RuntimeCore.h"
#include <filesystem>

SceneManager::~SceneManager(){ clearScene(); }

void SceneManager::setScene(std::unique_ptr<IScene> scene, ID3D12Device* device){
    if (m_editingPrefab) exitPrefabEdit();
    clearScene();
    if (scene && scene->initialize(device)){ activeScene = std::move(scene); activeScene->onEnter(); }
}

void SceneManager::clearScene(){
    if (m_editingPrefab) exitPrefabEdit();
    if (activeScene){
        if (auto* d3d = app->getD3D12()) d3d->flush();
        activeScene->onExit(); activeScene->shutdown(); activeScene.reset();
    }
    state = PlayState::Stopped;
    hasSerializedState = false;
    m_currentScenePath.clear();
    m_pendingScenePath.clear();
    m_pendingSpawns.clear();
    m_runtimeSceneChanged = false;
    m_renderOverrides.reset();
    m_runtimeTime.timeScale = 1.f;
    m_transition = {};
}

// SceneManager::getModuleScene() and requestSceneLoad()/requestPrefabSpawn()
// live in SceneManagerCore.cpp now - kept separate from this file's
// render()/updateAnimations()/play()/loadScene() so they can be linked into
// GameScript.dll (via PhoenixCore) without
// ComponentMesh/ComponentAnimation/SceneSerializer/ModuleD3D12.

// The active game camera is a raw GameObject*; drop it before its scene is replaced. A loaded scene
// re-sets it from whichever camera is saved with IsMainCamera.
static void clearActiveCamera(){
    if (ModuleCamera* cam = app->getCamera()){
        cam->setActiveCamera(nullptr);
        cam->clearGameCameraFrustum();
    }
}

void SceneManager::play(){
    if (!activeScene || m_editingPrefab) return;
    if (state == PlayState::Stopped){
        if (auto* ms = activeScene->getModuleScene()) hasSerializedState = SceneSerializer::SaveTempScene(ms);
        m_runtimeSceneChanged = false;
        m_scenePathAtPlay = m_currentScenePath;
        m_lookAtPlay = { settings.skybox, settings.ambient, settings.gravityY, settings.postProcess, settings.fog,
                          settings.xray, settings.occlusionFade };
        m_renderOverrides.reset();
        m_runtimeTime.timeScale = 1.f;
        m_pendingScenePath.clear();
        m_pendingSpawns.clear();
        m_transition = {};
    }
    state = PlayState::Playing;
}

void SceneManager::pause(){
    if (state == PlayState::Playing) state = PlayState::Paused;
    else if (state == PlayState::Paused) state = PlayState::Playing;
}

void SceneManager::stop(){
    if (!activeScene || state == PlayState::Stopped) return;

    if (auto* d3d = app->getD3D12()) d3d->flush();

    m_pendingScenePath.clear();
    m_pendingSpawns.clear();
    m_transition = {};   // Stop mid-fade: no overlay, no input lock
    // Play may have made a spawned object (e.g. a follow camera) the active camera; the temp scene
    // restore below destroys it.
    clearActiveCamera();

    auto* ms = activeScene->getModuleScene();
    if (hasSerializedState && ms){
        if (!SceneSerializer::LoadTempScene(ms)){ LOG("SceneManager: Failed to restore temp scene, falling back to reset()"); activeScene->reset(); }
        hasSerializedState = false;
    }
    else activeScene->reset();
    state = PlayState::Stopped;
    m_renderOverrides.reset();
    m_runtimeTime.timeScale = 1.f;

    // A script loaded other scenes during Play: the temp scene above only restores GameObjects, so put the
    // scene path and the look settings (skybox, fog, ...) of the scene Play started in back as well.
    if (m_runtimeSceneChanged){
        m_runtimeSceneChanged = false;
        m_currentScenePath = m_scenePathAtPlay;
        settings.skybox = m_lookAtPlay.skybox;
        settings.ambient = m_lookAtPlay.ambient;
        settings.gravityY = m_lookAtPlay.gravityY;
        settings.postProcess = m_lookAtPlay.postProcess;
        settings.fog = m_lookAtPlay.fog;
        settings.xray = m_lookAtPlay.xray;
        settings.occlusionFade = m_lookAtPlay.occlusionFade;
        if (RuntimeCore* rc = app->getRuntimeCore()) rc->applySkyboxFromSettings();
    }
}

bool SceneManager::processRuntimeRequests(){
    bool loaded = false;

    if (!m_pendingScenePath.empty()){
        const std::string path = m_pendingScenePath;
        m_pendingScenePath.clear();

        if (isTransitionActive()){
            // The transition owns the next scene change; a second, unfaded one would pop mid-fade.
            LOG("SceneManager: ignoring script scene load '%s' during a level transition", path.c_str());
        }
        else if (replaceScene(path)){
            loaded = true;
            LOG("SceneManager: script loaded scene %s", path.c_str());
        }
    }

    if (!m_pendingSpawns.empty()){
        std::vector<PendingSpawn> spawns = std::move(m_pendingSpawns);
        m_pendingSpawns.clear();
        SceneGraph* ms = getModuleScene();
        for (const PendingSpawn& s : spawns){
            GameObject* go = ms ? PrefabManager::instantiatePrefab(s.prefabName, ms) : nullptr;
            if (!go){ LOG("SceneManager: script prefab spawn failed: '%s'", s.prefabName.c_str()); continue; }
            ComponentTransform* t = go->getTransform();
            t->position = s.position;
            t->rotation = s.rotation;
            t->markDirty();
        }
    }

    return loaded;
}

void SceneManager::update(float deltaTime){
    if (m_editingPrefab) return;
    if (activeScene && state == PlayState::Playing) activeScene->update(deltaTime);
}

void SceneManager::updateAnimations(float deltaTime){
    if (m_editingPrefab) return;
    if (state == PlayState::Playing) return;
    auto* ms = getModuleScene();
    if (!ms) return;
    std::function<void(GameObject*)> visit = [&](GameObject* go){
        if (!go || !go->isActive()) return;
        if (auto* anim = go->getComponent<ComponentAnimation>()) anim->update(deltaTime);
        for (auto* child : go->getChildren()) visit(child);
    };
    visit(ms->getRoot());
}

static void renderModuleScene(SceneGraph* ms, ID3D12GraphicsCommandList* cmd){
    if (!ms) return;
    std::function<void(GameObject*)> visit = [&](GameObject* node){
        if (!node || !node->isActive()) return;
        if (auto* mesh = node->getComponent<ComponentMesh>()) mesh->render(cmd);
        for (auto* child : node->getChildren()) visit(child);
        };
    visit(ms->getRoot());
}

void SceneManager::render(ID3D12GraphicsCommandList* cmd, const ModuleCamera& camera, uint32_t w, uint32_t h){
    if (m_editingPrefab){
        renderModuleScene(m_prefabScene, cmd);
        return;
    }
    if (activeScene) activeScene->render(cmd, camera, w, h);
}

void SceneManager::onViewportResized(uint32_t w, uint32_t h){
    if (activeScene) activeScene->onViewportResized(w, h);
}

bool SceneManager::saveCurrentScene(const std::string& filePath){
    if (m_editingPrefab){ LOG("SceneManager: Cannot save scene while editing a prefab"); return false; }
    auto* ms = activeScene ? activeScene->getModuleScene() : nullptr;
    if (!ms){ LOG("SceneManager: No active scene to save"); return false; }
    // The editor's "current scene" file is still the one Play started in; saving now would write another
    // level's contents over it.
    if (m_runtimeSceneChanged){ LOG("SceneManager: Cannot save - a script changed scenes during Play. Stop first."); return false; }
    if (!SceneSerializer::SaveScene(ms, filePath, &settings)) return false;
    if (state == PlayState::Stopped) m_currentScenePath = filePath;
    return true;
}

bool SceneManager::loadScene(const std::string& filePath){
    if (m_editingPrefab){ LOG("SceneManager: Cannot load scene while editing a prefab"); return false; }
    auto* ms = activeScene ? activeScene->getModuleScene() : nullptr;
    if (!ms){ LOG("SceneManager: No active scene to load into"); return false; }
    // The old scene's meshes, textures and material buffers are freed as its GameObjects are destroyed, with
    // no fence tracking of their own: wait for every submitted frame that could still reference them. Callers
    // load between frames (no command list open), so this covers everything in flight.
    app->getD3D12()->flush();
    if (!SceneSerializer::LoadScene(filePath, ms, &settings)) return false;
    m_currentScenePath = filePath;
    m_renderOverrides.reset();
    m_runtimeTime.timeScale = 1.f;
    return true;
}

bool SceneManager::replaceScene(const std::string& filePath){
    m_pendingSpawns.clear();   // queued for the scene being left
    if (m_editingPrefab){
        LOG("SceneManager: ignoring scene load '%s' while editing a prefab", filePath.c_str());
        return false;
    }
    if (!app->getFileSystem()->Exists(filePath.c_str())){
        LOG("SceneManager: scene load failed, file not found: %s", filePath.c_str());
        return false;
    }
    if (m_onRuntimeSceneChange) m_onRuntimeSceneChange();
    clearActiveCamera();
    if (!loadScene(filePath)){
        LOG("SceneManager: scene load failed: %s", filePath.c_str());
        return false;
    }
    if (state != PlayState::Stopped) m_runtimeSceneChanged = true;
    return true;
}

bool SceneManager::loadSceneByBuildIndex(int index, const BuildSettings& buildSettings){
    std::string path = buildSettings.getScenePathAtBuildIndex(index);
    if (path.empty()){ LOG("SceneManager: No scene at build index %d", index); return false; }
    if (!std::filesystem::path(path).is_absolute()){
        std::string assetsPath = app->getFileSystem()->GetAssetsPath();
        std::string baseDir = assetsPath.substr(0, assetsPath.size() - std::string("Assets/").size());
        path = baseDir + path;
    }
    return loadScene(path);
}

void SceneManager::enterPrefabEdit(SceneGraph* prefabScene, const std::string& prefabName){
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
