#pragma once
#include "EditorSceneSettings.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <d3d12.h>

class IScene;
class ModuleCamera;
class SceneGraph;
struct BuildSettings;

class SceneManager {
public:
    enum class PlayState { Stopped, Playing, Paused };

    SceneManager() = default;
    ~SceneManager();

    void setScene(std::unique_ptr<IScene> scene, ID3D12Device* device);
    void clearScene();

    void play();
    void pause();
    void stop();

    PlayState getState() const { return state; }
    bool isPlaying() const { return state == PlayState::Playing; }

    void update(float deltaTime);
    void updateAnimations(float deltaTime);
    void render(ID3D12GraphicsCommandList* cmd, const ModuleCamera& camera, uint32_t width, uint32_t height);
    void onViewportResized(uint32_t width, uint32_t height);

    IScene* getActiveScene() const { return activeScene.get(); }
    SceneGraph* getModuleScene() const;

    bool saveCurrentScene(const std::string& filePath);
    bool loadScene(const std::string& filePath);
    bool loadSceneByBuildIndex(int index, const BuildSettings& buildSettings);

    // Path of the scene file last loaded (or saved) into the active scene; empty for an unsaved scene.
    const std::string& getCurrentScenePath() const { return m_currentScenePath; }

    // Script-driven requests (Phoenix::Scene::LoadScene / InstantiatePrefab). Both are deferred to
    // processRuntimeRequests(), after the scene update, so a script can ask from inside its own Update()
    // without the scene being torn down or its children re-allocated mid-traversal. A scene load drops any
    // prefab spawns still queued for the scene being left. The request functions live in SceneManagerCore.cpp
    // so they link into GameScript.dll.
    void requestSceneLoad(const std::string& filePath);
    void requestPrefabSpawn(const std::string& prefabName, const Vector3& position, const Quaternion& rotation);
    // Returns true if it loaded a scene (the caller re-applies the skybox).
    bool processRuntimeRequests();

    // Called right before a script-requested scene load replaces every GameObject, so tools holding
    // GameObject pointers (editor selection, undo) can drop them.
    void setOnRuntimeSceneChange(std::function<void()> callback){ m_onRuntimeSceneChange = std::move(callback); }

    void enterPrefabEdit(SceneGraph* prefabScene, const std::string& prefabName);
    void exitPrefabEdit();
    bool isEditingPrefab() const { return m_editingPrefab; }
    const std::string& getPrefabEditName() const { return m_prefabEditName; }

    EditorSceneSettings& getSettings(){ return settings; }
    const EditorSceneSettings& getSettings() const { return settings; }

private:
    struct PendingSpawn {
        std::string prefabName;
        Vector3 position;
        Quaternion rotation;
    };

    // The parts of `settings` a scene file overwrites, captured at play() so stop() can put them back after
    // a script moved Play mode to another scene.
    struct SceneLook {
        EditorSceneSettings::Skybox skybox;
        EditorSceneSettings::Ambient ambient;
        float gravityY = -9.81f;
        EditorSceneSettings::PostProcess postProcess;
        EditorSceneSettings::Fog fog;
    };

    std::unique_ptr<IScene> activeScene;
    PlayState state = PlayState::Stopped;
    bool hasSerializedState = false;
    EditorSceneSettings settings;

    bool m_editingPrefab = false;
    std::string m_prefabEditName;
    SceneGraph* m_savedScene = nullptr;
    SceneGraph* m_prefabScene = nullptr;

    std::string m_currentScenePath;
    std::string m_pendingScenePath;
    std::vector<PendingSpawn> m_pendingSpawns;
    std::function<void()> m_onRuntimeSceneChange;

    // Play-session bookkeeping for script-driven scene changes.
    bool m_runtimeSceneChanged = false;
    std::string m_scenePathAtPlay;
    SceneLook m_lookAtPlay;
};
