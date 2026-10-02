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

// A faded level change (Phoenix::Scene::RequestLevelTransition), driven one step per frame by the engine's
// SceneTransition controller. Lives here, as plain data with inline getters, so GameScript.dll can read the
// phase and the input lock without linking the controller.
struct SceneTransitionState {
    enum class Phase { Idle, FadingOut, HoldBlack, Loading, WarmUp, FadingIn };
    Phase phase = Phase::Idle;
    float fadeAlpha = 0.f;          // black overlay opacity drawn over the game view, 0..1
    std::string requestedPath;      // set by requestTransition(), taken by the controller when it starts
    bool readySignalled = false;    // the new level said its player and camera are in place (NotifyTransitionReady)
};

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
    // loadScene() for a scene that replaces the running one: first tells tools holding GameObject pointers
    // (setOnRuntimeSceneChange) and drops the active camera, and during Play records that the scene changed so
    // stop() restores the one Play started in. Drops queued prefab spawns (they belong to the scene being left).
    bool replaceScene(const std::string& filePath);
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
    // Returns true if it loaded a scene (the caller re-applies the skybox). A LoadScene request made while a
    // level transition is running is dropped.
    bool processRuntimeRequests();

    // Faded level transitions. requestTransition() only records the target (false, and nothing recorded, while
    // another transition is running or a prefab is being edited); the engine's SceneTransition controller runs
    // the fade, the load and the warm-up. Both live in SceneManagerCore.cpp so they link into GameScript.dll.
    bool requestTransition(const std::string& filePath);
    void notifyTransitionReady();
    SceneTransitionState& getTransition(){ return m_transition; }
    const SceneTransitionState& getTransition() const { return m_transition; }
    bool isTransitionActive() const {
        return m_transition.phase != SceneTransitionState::Phase::Idle || !m_transition.requestedPath.empty();
    }
    // Player input is locked from the request until the fade-in begins.
    bool isTransitionInputLocked() const {
        return isTransitionActive() && m_transition.phase != SceneTransitionState::Phase::FadingIn;
    }

    // Called right before replaceScene() swaps every GameObject out (a script's LoadScene, a level transition, or
    // an editor scene load), so tools holding GameObject pointers (editor selection, undo) can drop them.
    void setOnRuntimeSceneChange(std::function<void()> callback){ m_onRuntimeSceneChange = std::move(callback); }

    void enterPrefabEdit(SceneGraph* prefabScene, const std::string& prefabName);
    void exitPrefabEdit();
    bool isEditingPrefab() const { return m_editingPrefab; }
    const std::string& getPrefabEditName() const { return m_prefabEditName; }

    EditorSceneSettings& getSettings(){ return settings; }
    const EditorSceneSettings& getSettings() const { return settings; }

    // Runtime overrides written by Phoenix::Render (inline so GameScript.dll writes the engine's instance).
    RenderOverrides& getRenderOverrides(){ return m_renderOverrides; }
    const RenderOverrides& getRenderOverrides() const { return m_renderOverrides; }

    // Time scale and real-time delta (Phoenix::VFX), inline for the same reason.
    RuntimeTime& getRuntimeTime(){ return m_runtimeTime; }
    const RuntimeTime& getRuntimeTime() const { return m_runtimeTime; }
    EngineHooks& getEngineHooks(){ return m_engineHooks; }

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
        EditorSceneSettings::XRay xray;
        EditorSceneSettings::OcclusionFade occlusionFade;
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

    RenderOverrides m_renderOverrides;
    RuntimeTime m_runtimeTime;
    EngineHooks m_engineHooks;

    SceneTransitionState m_transition;
};
