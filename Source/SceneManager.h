#pragma once

#include <memory>
#include <d3d12.h>

class IScene;
class ModuleCamera;

class SceneManager
{
public:
    enum class PlayState
    {
        Stopped,
        Playing,
        Paused
    };
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
    void render(
        ID3D12GraphicsCommandList* cmd,
        const ModuleCamera& camera,
        uint32_t width,
        uint32_t height
    );

    void onViewportResized(uint32_t width, uint32_t height);

private:
    std::unique_ptr<IScene> activeScene;
    PlayState state = PlayState::Stopped;
};