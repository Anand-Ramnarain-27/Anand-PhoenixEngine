#pragma once

#include "EditorSceneSettings.h"
#include <memory>
#include <d3d12.h>

class IScene;
class ModuleCamera;

class SceneManager
{
public:
    enum class PlayState { Stopped, Playing, Paused };

    SceneManager() = default;
    ~SceneManager();

    void setScene(std::unique_ptr<IScene> scene, ID3D12Device* device);
    void clearScene();

    void play();
    void pause();
    void stop();

    PlayState getState()   const { return state; }
    bool      isPlaying()  const { return state == PlayState::Playing; }

    void update(float deltaTime);
    void render(ID3D12GraphicsCommandList* cmd, const ModuleCamera& camera, uint32_t width, uint32_t height);
    void onViewportResized(uint32_t width, uint32_t height);

    IScene* getActiveScene() const { return activeScene.get(); }

    bool saveCurrentScene(const std::string& filePath);
    bool loadScene(const std::string& filePath);

    EditorSceneSettings& getSettings() { return settings; }
    const EditorSceneSettings& getSettings() const { return settings; }

private:
    std::unique_ptr<IScene> activeScene;
    PlayState               state = PlayState::Stopped;
    bool                    hasSerializedState = false;
    EditorSceneSettings     settings;
};