#include "Globals.h"
#include "API/Phoenix_Scene.h"
#include "Application.h"
#include "RuntimeCore.h"
#include "SceneManager.h"
#include "SceneGraph.h"
#include "ModuleCamera.h"
#include "GameObject.h"
#include "ComponentCamera.h"
#include "ModuleFileSystem.h"
#include <filesystem>
#include <algorithm>

namespace Phoenix {

static SceneGraph* getScene(){
    if (!app || !app->getRuntimeCore()) return nullptr;
    return app->getRuntimeCore()->getActiveModuleScene();
}

static SceneManager* getSceneManager(){
    if (!app || !app->getRuntimeCore()) return nullptr;
    return app->getRuntimeCore()->getSceneManager();
}

// "AF_KraugsDen" -> <Library>/Scenes/AF_KraugsDen.json; anything that already looks like a path is kept.
static std::string resolveScenePath(const std::string& nameOrPath){
    if (nameOrPath.empty()) return nameOrPath;
    if (nameOrPath.find_first_of("/\\") != std::string::npos ||
        std::filesystem::path(nameOrPath).extension() == ".json")
        return nameOrPath;
    return app->getFileSystem()->GetLibraryPath() + "Scenes/" + nameOrPath + ".json";
}

GameObject* Scene::Find(const std::string& name){
    if (SceneGraph* sg = getScene())
        return sg->findGameObjectByName(name);
    return nullptr;
}

GameObject* Scene::Spawn(const std::string& name){
    if (SceneGraph* sg = getScene())
        return sg->createGameObject(name);
    return nullptr;
}

void Scene::Destroy(GameObject* go){
    if (!go) return;
    if (SceneGraph* sg = getScene())
        sg->destroyGameObject(go);
}

void Scene::LoadScene(const std::string& sceneNameOrPath){
    if (SceneManager* sm = getSceneManager())
        sm->requestSceneLoad(resolveScenePath(sceneNameOrPath));
}

bool Scene::SceneExists(const std::string& sceneNameOrPath){
    if (!app || sceneNameOrPath.empty()) return false;
    return app->getFileSystem()->Exists(resolveScenePath(sceneNameOrPath).c_str());
}

bool Scene::RequestLevelTransition(const std::string& sceneNameOrPath){
    SceneManager* sm = getSceneManager();
    if (!sm) return false;
    if (!SceneExists(sceneNameOrPath)){
        LOG("Phoenix::Scene: transition target '%s' not found", sceneNameOrPath.c_str());
        return false;
    }
    return sm->requestTransition(resolveScenePath(sceneNameOrPath));
}

bool Scene::IsTransitionActive(){
    SceneManager* sm = getSceneManager();
    return sm && sm->isTransitionActive();
}

bool Scene::IsTransitionInputLocked(){
    SceneManager* sm = getSceneManager();
    return sm && sm->isTransitionInputLocked();
}

void Scene::NotifyTransitionReady(){
    if (SceneManager* sm = getSceneManager()) sm->notifyTransitionReady();
}

void Scene::InstantiatePrefab(const std::string& prefabName, Vec3 position, Quat rotation){
    if (SceneManager* sm = getSceneManager())
        sm->requestPrefabSpawn(prefabName, position, rotation);
}

std::string Scene::GetActiveScenePath(){
    SceneManager* sm = getSceneManager();
    return sm ? sm->getCurrentScenePath() : std::string();
}

std::string Scene::GetActiveSceneName(){
    const std::string path = GetActiveScenePath();
    return path.empty() ? path : std::filesystem::path(path).stem().string();
}

GameObject* Scene::GetActiveCamera(){
    ModuleCamera* cam = app ? app->getCamera() : nullptr;
    return cam ? cam->getActiveCamera() : nullptr;
}

void Scene::SetActiveCamera(GameObject* cameraObject){
    ModuleCamera* cam = app ? app->getCamera() : nullptr;
    if (!cam) return;
    cam->setActiveCamera(cameraObject);
    if (!cameraObject) cam->clearGameCameraFrustum();
}

static ComponentCamera* getActiveCameraComponent(){
    GameObject* go = Scene::GetActiveCamera();
    return go ? go->getComponent<ComponentCamera>() : nullptr;
}

float Scene::GetActiveCameraFOV(){
    ComponentCamera* cam = getActiveCameraComponent();
    return cam ? cam->getFOV() : 0.f;
}

void Scene::SetActiveCameraFOV(float radians){
    if (ComponentCamera* cam = getActiveCameraComponent())
        cam->setFOV(std::clamp(radians, 0.0174533f, 3.0f));   // 1 deg .. ~172 deg
}

} // namespace Phoenix
