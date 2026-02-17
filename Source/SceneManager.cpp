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
    hasSerializedState = false;
}

void SceneManager::play()
{
    if (!activeScene)
        return;

    if (state == PlayState::Stopped)
    {
        ModuleScene* moduleScene = activeScene->getModuleScene();
        if (moduleScene)
        {
            LOG("SceneManager: Serializing scene state before play");
            hasSerializedState = SceneSerializer::SaveTempScene(moduleScene);

            if (!hasSerializedState)
            {
                LOG("SceneManager: Warning - Failed to serialize scene state!");
            }
            else
            {
                LOG("SceneManager: Scene state saved successfully");
            }
        }

        activeScene->reset();
    }

    state = PlayState::Playing;
    LOG("SceneManager: Playing");
}

void SceneManager::pause()
{
    if (state == PlayState::Playing)
    {
        state = PlayState::Paused;
        LOG("SceneManager: Paused");
    }
}

void SceneManager::stop()
{
    if (!activeScene)
        return;

    LOG("SceneManager: Stopping");

    if (hasSerializedState)
    {
        LOG("SceneManager: Restoring scene state from serialization");

        ModuleScene* moduleScene = activeScene->getModuleScene();
        if (moduleScene)
        {
            activeScene->onExit();

            if (SceneSerializer::LoadTempScene(moduleScene))
            {
                LOG("SceneManager: Scene state restored successfully");
            }
            else
            {
                LOG("SceneManager: Error - Failed to restore scene state!");
                activeScene->reset();
            }

            activeScene->onEnter();
        }

        hasSerializedState = false;
    }
    else
    {
        LOG("SceneManager: No serialized state, performing reset");
        activeScene->onExit();
        activeScene->reset();
        activeScene->onEnter();
    }

    state = PlayState::Stopped;
    LOG("SceneManager: Stopped");
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

bool SceneManager::saveCurrentScene(const std::string& filePath)
{
    if (!activeScene)
    {
        LOG("SceneManager: No active scene to save");
        return false;
    }

    ModuleScene* moduleScene = activeScene->getModuleScene();
    if (!moduleScene)
    {
        LOG("SceneManager: Active scene does not have a ModuleScene");
        return false;
    }

    LOG("SceneManager: Saving scene to %s", filePath.c_str());
    return SceneSerializer::SaveScene(moduleScene, filePath);
}

bool SceneManager::loadScene(const std::string& filePath)
{
    if (!activeScene)
    {
        LOG("SceneManager: No active scene to load into");
        return false;
    }

    ModuleScene* moduleScene = activeScene->getModuleScene();
    if (!moduleScene)
    {
        LOG("SceneManager: Active scene does not have a ModuleScene");
        return false;
    }

    app->getD3D12()->flush();

    LOG("SceneManager: Loading scene from %s", filePath.c_str());
    return SceneSerializer::LoadScene(filePath, moduleScene);
}