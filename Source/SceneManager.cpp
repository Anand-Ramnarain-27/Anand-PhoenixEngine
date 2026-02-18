#include "Globals.h"
#include "SceneManager.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "IScene.h"
#include "ModuleScene.h"
#include "SceneSerializer.h"

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
    hasSerializedState = false;
}

void SceneManager::play()
{
    if (!activeScene) return;

    if (state == PlayState::Stopped)
    {
        ModuleScene* moduleScene = activeScene->getModuleScene();
        if (moduleScene)
            hasSerializedState = SceneSerializer::SaveTempScene(moduleScene);
    }

    state = PlayState::Playing;
}

void SceneManager::pause()
{
    if (state == PlayState::Playing)
        state = PlayState::Paused;
    else if (state == PlayState::Paused)
        state = PlayState::Playing;
}

void SceneManager::stop()
{
    if (!activeScene || state == PlayState::Stopped) return;

    ModuleScene* moduleScene = activeScene->getModuleScene();

    if (hasSerializedState && moduleScene)
    {
        if (!SceneSerializer::LoadTempScene(moduleScene))
        {
            LOG("SceneManager: Failed to restore temp scene, falling back to reset()");
            activeScene->reset();
        }
        hasSerializedState = false;
    }
    else
    {
        activeScene->reset();
    }

    state = PlayState::Stopped;
}

void SceneManager::update(float deltaTime)
{
    if (activeScene && state == PlayState::Playing)
        activeScene->update(deltaTime);
}

void SceneManager::render(ID3D12GraphicsCommandList* cmd, const ModuleCamera& camera, uint32_t width, uint32_t height)
{
    if (activeScene) activeScene->render(cmd, camera, width, height);
}

void SceneManager::onViewportResized(uint32_t width, uint32_t height)
{
    if (activeScene) activeScene->onViewportResized(width, height);
}

bool SceneManager::saveCurrentScene(const std::string& filePath)
{
    ModuleScene* moduleScene = activeScene ? activeScene->getModuleScene() : nullptr;
    if (!moduleScene)
    {
        LOG("SceneManager: No active scene to save");
        return false;
    }
    return SceneSerializer::SaveScene(moduleScene, filePath);
}

bool SceneManager::loadScene(const std::string& filePath)
{
    ModuleScene* moduleScene = activeScene ? activeScene->getModuleScene() : nullptr;
    if (!moduleScene)
    {
        LOG("SceneManager: No active scene to load into");
        return false;
    }
    app->getD3D12()->flush();
    return SceneSerializer::LoadScene(filePath, moduleScene);
}