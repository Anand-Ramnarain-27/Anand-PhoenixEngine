#include "Globals.h"
#include "API/Phoenix_Scene.h"
#include "Application.h"
#include "ModuleEditor.h"
#include "SceneGraph.h"

namespace Phoenix {

static SceneGraph* getScene(){
    if (!app || !app->getEditor()) return nullptr;
    return app->getEditor()->getActiveModuleScene();
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

} // namespace Phoenix
