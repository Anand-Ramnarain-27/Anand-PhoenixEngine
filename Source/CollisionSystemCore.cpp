// Thin half of CollisionSystem - body-gathering and the on-demand Raycast
// query - split out so PhoenixCore/GameScript.dll can call Raycast (e.g. for
// AI line-of-sight) without this file's broad/mid/narrow-phase pipeline.
// Mirrors the existing SceneManager/SceneManagerCore split.
#include "Globals.h"
#include "CollisionSystem.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "ComponentMesh.h"
#include "ComponentTransform.h"
#include "ComponentBounds.h"
#include "ComponentRigidbody.h"
#include "RayMath.h"
#include <functional>
#include <cfloat>
#include <cmath>

static void buildOBB(CollisionBody& body){
    const ComponentTransform* t = body.go->getTransform();
    const ComponentMesh* cm = body.go->getComponent<ComponentMesh>();
    if (!t || !cm || !cm->hasAABB()) return;

    const Matrix& W = const_cast<ComponentTransform*>(t)->getGlobalMatrix();
    const Vector3 lMin = cm->getLocalAABBMin();
    const Vector3 lMax = cm->getLocalAABBMax();
    const Vector3 lHalf = (lMax - lMin) * 0.5f;
    const Vector3 lCtr = (lMin + lMax) * 0.5f;

    body.obbCenter = Vector3::Transform(lCtr, W);

    Vector3 cx(W._11, W._12, W._13);
    Vector3 cy(W._21, W._22, W._23);
    Vector3 cz(W._31, W._32, W._33);

    float sx = cx.Length(), sy = cy.Length(), sz = cz.Length();
    static const float kEps = 1e-8f;
    body.obbAxes[0] = sx > kEps ? cx / sx : Vector3::UnitX;
    body.obbAxes[1] = sy > kEps ? cy / sy : Vector3::UnitY;
    body.obbAxes[2] = sz > kEps ? cz / sz : Vector3::UnitZ;
    body.obbHalves[0] = lHalf.x * sx;
    body.obbHalves[1] = lHalf.y * sy;
    body.obbHalves[2] = lHalf.z * sz;
}

static void applyBVType(CollisionBody& body){
    const ComponentBounds* cb = body.go->getComponent<ComponentBounds>();
    if (!cb || cb->bvType == BVType::AABB){
        body.bvType = BVType::AABB;
        return;
    }

    body.bvType = BVType::Sphere;
    body.sphereCenter = body.obbCenter;

    if (cb->radiusOverride >= 0.f){
        body.sphereRadius = cb->radiusOverride;
    } else {
        body.sphereRadius = sqrtf(
            body.obbHalves[0] * body.obbHalves[0] +
            body.obbHalves[1] * body.obbHalves[1] +
            body.obbHalves[2] * body.obbHalves[2]);
    }

    Sphere s{ body.sphereCenter, body.sphereRadius };
    body.worldAABB = s.toAABB();
}

std::vector<CollisionBody> CollisionSystem::gatherBodies(SceneGraph* scene, float dt){
    std::vector<CollisionBody> bodies;
    if (!scene) return bodies;

    std::function<void(GameObject*)> visit = [&](GameObject* node){
        if (!node || !node->isActive()) return;
        ComponentMesh* cm = node->getComponent<ComponentMesh>();
        if (cm && cm->hasAABB()){
            CollisionBody body;
            body.go = node;
            Vector3 mn, mx;
            cm->getWorldAABB(mn, mx);
            body.worldAABB.min = mn;
            body.worldAABB.max = mx;
            buildOBB(body);
            applyBVType(body);

            const ComponentRigidbody* rb = node->getComponent<ComponentRigidbody>();
            if (rb && rb->isFastMoving && !rb->isStatic && dt > 1e-7f){
                const Vector3 disp = rb->velocity * dt;
                body.worldAABB.min = Vector3::Min(body.worldAABB.min,
                                                   body.worldAABB.min + disp);
                body.worldAABB.max = Vector3::Max(body.worldAABB.max,
                                                   body.worldAABB.max + disp);
            }

            bodies.push_back(body);
        }
        for (auto* child : node->getChildren()) visit(child);
    };
    visit(scene->getRoot());
    return bodies;
}

bool CollisionSystem::Raycast(SceneGraph* scene, const Vector3& origin, const Vector3& dir,
                              float maxDistance, RaycastHit& outHit){
    outHit = RaycastHit{};
    if (!scene || maxDistance <= 0.f) return false;

    Vector3 d = dir;
    if (d.LengthSquared() < 1e-8f) return false;
    d.Normalize();
    RayMath::Ray ray{ origin, d };

    std::vector<CollisionBody> bodies = gatherBodies(scene, 0.f);

    float closest = maxDistance;
    for (const auto& body : bodies){
        if (RayMath::RayVsAABB(ray, body.worldAABB.min, body.worldAABB.max, closest) == FLT_MAX)
            continue;

        Vector3 normal;
        float t;
        if (body.bvType == BVType::Sphere){
            t = RayMath::RayVsSphere(ray, body.sphereCenter, body.sphereRadius, closest);
            if (t == FLT_MAX) continue;
            Vector3 hitPoint = origin + d * t;
            normal = hitPoint - body.sphereCenter;
            normal.Normalize();
        } else {
            t = RayMath::RayVsOBB(ray, body.obbCenter, body.obbAxes, body.obbHalves, closest, &normal);
            if (t == FLT_MAX) continue;
        }

        if (t < closest){
            closest = t;
            outHit.hit = true;
            outHit.distance = t;
            outHit.point = origin + d * t;
            outHit.normal = normal;
            outHit.object = body.go;
        }
    }

    return outHit.hit;
}

bool CollisionSystem::IsLineClear(SceneGraph* scene, const Vector3& from, const Vector3& to){
    Vector3 delta = to - from;
    float dist = delta.Length();
    if (dist < 1e-5f) return true;

    RaycastHit hit;
    if (!Raycast(scene, from, delta, dist, hit)) return true;
    return hit.distance >= dist - 1e-4f;
}
