#include "Globals.h"
#include "MousePicker.h"
#include "SceneGraph.h"
#include "GameObject.h"
#include "ComponentMesh.h"
#include "ComponentTransform.h"
#include "MeshEntry.h"
#include "Mesh.h"
#include "ResourceMesh.h"
#include <cfloat>
#include <cmath>

RayMath::Ray MousePicker::buildRay(
    float mx, float my,
    float vpX, float vpY, float vpW, float vpH,
    const Matrix& view, const Matrix& proj){
    float ndcX = (2.f * (mx - vpX) / vpW) - 1.f;
    float ndcY = -(2.f * (my - vpY) / vpH) + 1.f;

    Matrix invVP = (view * proj).Invert();

    auto unproject = [&](float z) -> Vector3 {
        Vector4 clip(ndcX, ndcY, z, 1.f);
        Vector4 w = Vector4::Transform(clip, invVP);
        return Vector3(w.x / w.w, w.y / w.w, w.z / w.w);
    };

    Vector3 nearPt = unproject(0.f);
    Vector3 farPt = unproject(1.f);

    RayMath::Ray r;
    r.origin = nearPt;
    r.direction = farPt - nearPt;
    r.direction.Normalize();
    return r;
}

float MousePicker::testMeshTriangles(const RayMath::Ray& ray, GameObject* go){
    ComponentMesh* cm = go->getComponent<ComponentMesh>();
    if (!cm) return FLT_MAX;

    ComponentTransform* t = go->getTransform();
    if (!t) return FLT_MAX;

    Matrix world = t->getGlobalMatrix();
    Matrix worldInv = world.Invert();

    RayMath::Ray localRay;
    localRay.origin = Vector3::Transform(ray.origin, worldInv);
    localRay.direction = Vector3::TransformNormal(ray.direction, worldInv);
    localRay.direction.Normalize();

    float closest = FLT_MAX;

    for (auto& entry : cm->getEntries()){
        Mesh* mesh = entry.mesh;
        if (!mesh && entry.meshRes) mesh = entry.meshRes->getMesh();
        if (!mesh) continue;

        const auto& verts = mesh->getVertices();
        const auto& indices = mesh->getIndices();
        for (size_t i = 0; i + 2 < indices.size(); i += 3){
            const Vector3& v0 = verts[indices[i ]].position;
            const Vector3& v1 = verts[indices[i + 1]].position;
            const Vector3& v2 = verts[indices[i + 2]].position;
            float d = RayMath::RayVsTriangle(localRay, v0, v1, v2);
            if (d < closest) closest = d;
        }
    }

    return closest;
}

void MousePicker::traverse(const RayMath::Ray& ray, GameObject* node,
                            float& outDist, GameObject*& outHit){
    if (!node || !node->isActive()) return;

    ComponentMesh* cm = node->getComponent<ComponentMesh>();
    if (cm && cm->hasAABB()){
        Vector3 wMin, wMax;
        cm->getWorldAABB(wMin, wMax);
        float aabbDist = RayMath::RayVsAABB(ray, wMin, wMax);
        if (aabbDist < outDist){
            float triDist = testMeshTriangles(ray, node);
            if (triDist < outDist){
                outDist = triDist;
                outHit = node;
            }
        }
    }

    for (GameObject* child : node->getChildren())
        traverse(ray, child, outDist, outHit);
}

GameObject* MousePicker::pick(
    float mx, float my,
    float vpX, float vpY, float vpW, float vpH,
    const Matrix& view, const Matrix& proj,
    SceneGraph* scene){
    if (!scene || vpW <= 0.f || vpH <= 0.f) return nullptr;

    RayMath::Ray ray = buildRay(mx, my, vpX, vpY, vpW, vpH, view, proj);

    float closest = FLT_MAX;
    GameObject* hit = nullptr;
    traverse(ray, scene->getRoot(), closest, hit);
    return hit;
}
