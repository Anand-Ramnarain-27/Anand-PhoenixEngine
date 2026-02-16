#include "Globals.h"
#include "SceneManager.h"
#include "IScene.h"

SceneManager::~SceneManager()
{
    clearScene();
}

void SceneManager::setScene(std::unique_ptr<IScene> scene, ID3D12Device* device)
{
    clearScene();

    if (scene && scene->initialize(device))
    {
        activeScene = std::move(scene);
        state = PlayState::Stopped;
        activeScene->onEnter();
    }
}

void SceneManager::clearScene()
{
    if (activeScene)
    {
        activeScene->onExit();
        activeScene->shutdown();
        activeScene.reset();
    }

    state = PlayState::Stopped;
}

void SceneManager::play()
{
    if (!activeScene)
        return;

    if (state == PlayState::Stopped)
        activeScene->reset();

    state = PlayState::Playing;
}

void SceneManager::pause()
{
    if (state == PlayState::Playing)
        state = PlayState::Paused;
}

void SceneManager::stop()
{
    if (!activeScene)
        return;

    activeScene->onExit();
    activeScene->reset();
    activeScene->onEnter();

    state = PlayState::Stopped;
}

void SceneManager::update(float deltaTime)
{
    if (activeScene && state == PlayState::Playing)
        activeScene->update(deltaTime);
}

void SceneManager::render(
    ID3D12GraphicsCommandList* cmd,
    const ModuleCamera& camera,
    uint32_t width,
    uint32_t height)
{
    if (activeScene)
        activeScene->render(cmd, camera, width, height);
}

void SceneManager::onViewportResized(uint32_t width, uint32_t height)
{
    if (activeScene)
        activeScene->onViewportResized(width, height);
}
