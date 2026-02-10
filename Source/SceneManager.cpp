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
    }
}

void SceneManager::clearScene()
{
    if (activeScene)
    {
        activeScene->shutdown();
        activeScene.reset();
    }
}

void SceneManager::update(float deltaTime)
{
    if (activeScene)
        activeScene->update(deltaTime);
}

void SceneManager::render(ID3D12GraphicsCommandList* cmd, const ModuleCamera& camera, uint32_t width, uint32_t height)
{
    if (activeScene)
    {
        activeScene->render(cmd, camera, width, height);
    }
}

void SceneManager::onViewportResized(uint32_t width, uint32_t height)
{
    if (activeScene)
    {
        activeScene->onViewportResized(width, height);
    }
}

