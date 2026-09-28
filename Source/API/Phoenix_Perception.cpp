#include "Globals.h"
#include "API/Phoenix_Perception.h"
#include "Application.h"
#include "RuntimeCore.h"
#include "CollisionSystem.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "ComponentMesh.h"
#include "ComponentTransform.h"
#include <algorithm>
#include <functional>

namespace Phoenix {

static SceneGraph* getScene(){
    if (!app || !app->getRuntimeCore()) return nullptr;
    return app->getRuntimeCore()->getActiveModuleScene();
}

static RaycastResult raycast(Vec3 origin, Vec3 direction, float maxDistance, bool meshPrecise,
                             const std::function<bool(GameObject*)>& ignore = {}){
    RaycastResult result;
    SceneGraph* scene = getScene();
    if (!scene) return result;

    RaycastHit hit;
    if (CollisionSystem::Raycast(scene, origin, direction, maxDistance, hit, meshPrecise, ignore)){
        result.hit = hit.hit;
        result.point = hit.point;
        result.normal = hit.normal;
        result.distance = hit.distance;
        result.object = hit.object;
    }
    return result;
}

RaycastResult Perception::Raycast(Vec3 origin, Vec3 direction, float maxDistance){
    return raycast(origin, direction, maxDistance, false);
}

RaycastResult Perception::RaycastMesh(Vec3 origin, Vec3 direction, float maxDistance,
                                      const std::function<bool(GameObject*)>& ignore){
    return raycast(origin, direction, maxDistance, true, ignore);
}

std::vector<RaycastResult> Perception::RaycastMeshBatch(const std::vector<RayCast>& rays,
                                                        const std::function<bool(GameObject*)>& ignore){
    std::vector<RaycastResult> results(rays.size());
    SceneGraph* scene = getScene();
    if (!scene || rays.empty()) return results;

    std::vector<RayQuery> queries;
    queries.reserve(rays.size());
    for (const RayCast& r : rays) queries.push_back({ r.origin, r.direction, r.maxDistance });
    std::vector<RaycastHit> hits;
    CollisionSystem::RaycastBatch(scene, queries, hits, true, ignore);
    for (size_t i = 0; i < hits.size(); ++i){
        results[i].hit = hits[i].hit;
        results[i].point = hits[i].point;
        results[i].normal = hits[i].normal;
        results[i].distance = hits[i].distance;
        results[i].object = hits[i].object;
    }
    return results;
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

// Shared walk for OverlapBox/OverlapSphere. Mesh bounds come from ComponentMesh's world AABB (the same
// bounds the collision broad phase uses), so no collision pass has to have run this frame.
static std::vector<GameObject*> overlap(const std::string& tag,
                                        const std::function<bool(const Vec3&, const Vec3&)>& overlapsAABB,
                                        const std::function<bool(const Vec3&)>& containsPoint){
    std::vector<GameObject*> out;
    SceneGraph* scene = getScene();
    if (!scene) return out;

    auto meshOverlaps = [&](GameObject* go){
        ComponentMesh* cm = go->getComponent<ComponentMesh>();
        if (!cm || !cm->hasAABB()) return false;
        Vector3 mn, mx;
        cm->getWorldAABB(mn, mx);
        return overlapsAABB(mn, mx);
    };
    std::function<bool(GameObject*)> hierarchyOverlaps = [&](GameObject* go){
        if (!go->isActive()) return false;
        if (meshOverlaps(go)) return true;
        for (GameObject* child : go->getChildren())
            if (hierarchyOverlaps(child)) return true;
        return false;
    };
    std::function<void(GameObject*)> visit = [&](GameObject* node){
        if (!node->isActive()) return;
        if (node != scene->getRoot()){
            if (tag.empty()){
                if (meshOverlaps(node)) out.push_back(node);
            }
            else if (node->getTag() == tag){
                if (containsPoint(node->getTransform()->getGlobalMatrix().Translation()) || hierarchyOverlaps(node))
                    out.push_back(node);
            }
        }
        for (GameObject* child : node->getChildren()) visit(child);
    };
    visit(scene->getRoot());
    return out;
}

std::vector<GameObject*> Perception::OverlapBox(Vec3 center, Vec3 halfExtents, const std::string& tag){
    const Vec3 lo = center - halfExtents, hi = center + halfExtents;
    return overlap(tag,
        [&](const Vec3& mn, const Vec3& mx){
            return mn.x <= hi.x && mx.x >= lo.x && mn.y <= hi.y && mx.y >= lo.y && mn.z <= hi.z && mx.z >= lo.z;
        },
        [&](const Vec3& p){
            return p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y && p.z >= lo.z && p.z <= hi.z;
        });
}

std::vector<GameObject*> Perception::OverlapSphere(Vec3 center, float radius, const std::string& tag){
    const float r2 = radius * radius;
    return overlap(tag,
        [&](const Vec3& mn, const Vec3& mx){
            const Vec3 closest(std::clamp(center.x, mn.x, mx.x), std::clamp(center.y, mn.y, mx.y), std::clamp(center.z, mn.z, mx.z));
            return (closest - center).LengthSquared() <= r2;
        },
        [&](const Vec3& p){ return (p - center).LengthSquared() <= r2; });
}

} // namespace Phoenix
