#pragma once
#include "Globals.h"

class GameObject;
class SceneGraph;

class MousePicker {
public:
    static GameObject* pick(
        float mx, float my,
        float vpX, float vpY, float vpW, float vpH,
        const Matrix& view, const Matrix& proj,
        SceneGraph* scene);

private:
    struct Ray {
        Vector3 origin;
        Vector3 direction;
    };

    static Ray buildRay(float mx, float my,
                        float vpX, float vpY, float vpW, float vpH,
                        const Matrix& view, const Matrix& proj);

    static float rayVsAABB(const Ray& ray, const Vector3& mn, const Vector3& mx);
    static float rayVsTriangle(const Ray& ray, const Vector3& v0, const Vector3& v1, const Vector3& v2);

    static float testMeshTriangles(const Ray& ray, GameObject* go);

    static void traverse(const Ray& ray, GameObject* node,
                         float& outDist, GameObject*& outHit);
};
