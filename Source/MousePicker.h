#pragma once
#include "Globals.h"

class GameObject;
class ModuleScene;

// Casts a ray through the viewport from the mouse position and returns the
// closest GameObject hit, or nullptr if the ray missed everything.
class MousePicker {
public:
    static GameObject* pick(
        float mx, float my,
        float vpX, float vpY, float vpW, float vpH,
        const Matrix& view, const Matrix& proj,
        ModuleScene* scene);

private:
    struct Ray {
        Vector3 origin;
        Vector3 direction; // unit length
    };

    static Ray buildRay(float mx, float my,
                        float vpX, float vpY, float vpW, float vpH,
                        const Matrix& view, const Matrix& proj);

    // Returns distance along ray, or FLT_MAX on miss.
    static float rayVsAABB(const Ray& ray, const Vector3& mn, const Vector3& mx);
    static float rayVsTriangle(const Ray& ray, const Vector3& v0, const Vector3& v1, const Vector3& v2);

    // Tests ray vs all mesh triangles of go (in local space, ray transformed to local).
    // Returns closest triangle hit distance, or FLT_MAX on miss.
    static float testMeshTriangles(const Ray& ray, GameObject* go);

    // Recursive traversal; writes closest (dist, go) into outDist/outHit.
    static void traverse(const Ray& ray, GameObject* node,
                         float& outDist, GameObject*& outHit);
};
