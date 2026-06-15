#include "Globals.h"
#include "MousePicker.h"
#include "ModuleScene.h"
#include "GameObject.h"
#include "ComponentMesh.h"
#include "ComponentTransform.h"
#include "MeshEntry.h"
#include "Mesh.h"
#include "ResourceMesh.h"
#include <cfloat>
#include <cmath>

MousePicker::Ray MousePicker::buildRay(
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

    Ray r;
    r.origin = nearPt;
    r.direction = farPt - nearPt;
    r.direction.Normalize();
    return r;
}

float MousePicker::rayVsAABB(const Ray& ray, const Vector3& mn, const Vector3& mx){
    float tmin = 0.f, tmax = FLT_MAX;
    const float* ro = &ray.origin.x;
    const float* rd = &ray.direction.x;
    const float* lo = &mn.x;
    const float* hi = &mx.x;

    for (int i = 0; i < 3; ++i){
        if (std::abs(rd[i]) < 1e-8f){
            if (ro[i] < lo[i] || ro[i] > hi[i]) return FLT_MAX;
        } else {
            float invD = 1.f / rd[i];
            float t1 = (lo[i] - ro[i]) * invD;
            float t2 = (hi[i] - ro[i]) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tmin = tmin > t1 ? tmin : t1;
            tmax = tmax < t2 ? tmax : t2;
            if (tmin > tmax) return FLT_MAX;
        }
    }
    return tmin >= 0.f ? tmin : (tmax >= 0.f ? tmax : FLT_MAX);
}

float MousePicker::rayVsTriangle(const Ray& ray,
                                  const Vector3& v0,
                                  const Vector3& v1,
                                  const Vector3& v2){
    constexpr float kEps = 1e-8f;

    Vector3 e1 = v1 - v0;
    Vector3 e2 = v2 - v0;
    Vector3 h = ray.direction.Cross(e2);
    float a = e1.Dot(h);

    if (std::abs(a) < kEps) return FLT_MAX;

    float f = 1.f / a;
    Vector3 s = ray.origin - v0;
    float u = f * s.Dot(h);
    if (u < 0.f || u > 1.f) return FLT_MAX;

    Vector3 q = s.Cross(e1);
    float v = f * ray.direction.Dot(q);
    if (v < 0.f || u + v > 1.f) return FLT_MAX;

    float t = f * e2.Dot(q);
    return t > kEps ? t : FLT_MAX;
}

float MousePicker::testMeshTriangles(const Ray& ray, GameObject* go){
    ComponentMesh* cm = go->getComponent<ComponentMesh>();
    if (!cm) return FLT_MAX;

    ComponentTransform* t = go->getTransform();
    if (!t) return FLT_MAX;

    Matrix world = t->getGlobalMatrix();
    Matrix worldInv = world.Invert();

    Ray localRay;
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
            float d = rayVsTriangle(localRay, v0, v1, v2);
            if (d < closest) closest = d;
        }
    }

    return closest;
}

void MousePicker::traverse(const Ray& ray, GameObject* node,
                            float& outDist, GameObject*& outHit){
    if (!node || !node->isActive()) return;

    ComponentMesh* cm = node->getComponent<ComponentMesh>();
    if (cm && cm->hasAABB()){
        Vector3 wMin, wMax;
        cm->getWorldAABB(wMin, wMax);
        float aabbDist = rayVsAABB(ray, wMin, wMax);
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
    ModuleScene* scene){
    if (!scene || vpW <= 0.f || vpH <= 0.f) return nullptr;

    Ray ray = buildRay(mx, my, vpX, vpY, vpW, vpH, view, proj);

    float closest = FLT_MAX;
    GameObject* hit = nullptr;
    traverse(ray, scene->getRoot(), closest, hit);
    return hit;
}
