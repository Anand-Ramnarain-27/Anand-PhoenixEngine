#pragma once
#include "Globals.h"
#include "RayMath.h"

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
    static RayMath::Ray buildRay(float mx, float my,
                                 float vpX, float vpY, float vpW, float vpH,
                                 const Matrix& view, const Matrix& proj);

    static float testMeshTriangles(const RayMath::Ray& ray, GameObject* go);

    static void traverse(const RayMath::Ray& ray, GameObject* node,
                         float& outDist, GameObject*& outHit);
};
