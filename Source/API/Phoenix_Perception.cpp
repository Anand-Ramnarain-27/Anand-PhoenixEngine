#include "Globals.h"
#include "API/Phoenix_Perception.h"
#include "Application.h"
#include "RuntimeCore.h"
#include "CollisionSystem.h"
#include "SceneGraph.h"

namespace Phoenix {

static SceneGraph* getScene(){
    if (!app || !app->getRuntimeCore()) return nullptr;
    return app->getRuntimeCore()->getActiveModuleScene();
}

RaycastResult Perception::Raycast(Vec3 origin, Vec3 direction, float maxDistance){
    RaycastResult result;
    SceneGraph* scene = getScene();
    if (!scene) return result;

    RaycastHit hit;
    if (CollisionSystem::Raycast(scene, origin, direction, maxDistance, hit)){
        result.hit = hit.hit;
        result.point = hit.point;
        result.normal = hit.normal;
        result.distance = hit.distance;
        result.object = hit.object;
    }
    return result;
}

bool Perception::IsLineClear(Vec3 from, Vec3 to){
    SceneGraph* scene = getScene();
    if (!scene) return true;
    return CollisionSystem::IsLineClear(scene, from, to);
}

bool Perception::ValidateChargeLane(Vec3 start, Vec3 direction, float minLength, float maxLength){
    SceneGraph* scene = getScene();
    if (!scene) return false;

    RaycastHit hit;
    if (!CollisionSystem::Raycast(scene, start, direction, maxLength, hit))
        return true; // clear all the way out to maxLength
    return hit.distance >= minLength;
}

GameObject* Perception::FindNearestWithTag(const std::string& tag, Vec3 fromPosition, GameObject* exclude){
    SceneGraph* scene = getScene();
    if (!scene) return nullptr;
    return scene->findNearestGameObjectWithTag(tag, fromPosition, exclude);
}

} // namespace Phoenix
