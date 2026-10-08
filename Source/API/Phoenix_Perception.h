#pragma once
// Raycasts, line of sight and overlap queries for scripts.

#include "API/Phoenix_Types.h"
#include <functional>
#include <string>
#include <vector>

class GameObject;

namespace Phoenix {

/// Result of a ray query; `object` is the hit GameObject.
struct RaycastResult {
    bool hit = false;
    Vec3 point;
    Vec3 normal;
    float distance = 0.f;
    GameObject* object = nullptr;
};

/// One ray of a RaycastMeshBatch.
struct RayCast {
    Vec3 origin;
    Vec3 direction;
    float maxDistance = 0.f;
};

/// Ray, line-of-sight, nearest-tag and overlap queries against the active scene.
struct Perception {
    /// Hits mesh bounding boxes: cheap, but a box covers doorways, the whole staircase, etc.
    static RaycastResult Raycast(Vec3 origin, Vec3 direction, float maxDistance);
    /// Hits meshes' actual triangles (both faces) - for character movement over stairs and around props.
    /// Skinned meshes, and meshes without CPU-side geometry, still count by their bounding box.
    /// Objects for which `ignore` returns true are passed through (e.g. the caster's own body).
    static RaycastResult RaycastMesh(Vec3 origin, Vec3 direction, float maxDistance,
                                     const std::function<bool(GameObject*)>& ignore = {});
    /// RaycastMesh for many rays at once: the scene is scanned once for the whole batch rather than once per
    /// ray. Result i answers rays[i].
    static std::vector<RaycastResult> RaycastMeshBatch(const std::vector<RayCast>& rays,
                                                       const std::function<bool(GameObject*)>& ignore = {});
    static bool IsLineClear(Vec3 from, Vec3 to);

    /// Clear straight run of at least minLength (up to maxLength) from start.
    static bool ValidateChargeLane(Vec3 start, Vec3 direction, float minLength, float maxLength);

    static GameObject* FindNearestWithTag(const std::string& tag, Vec3 fromPosition, GameObject* exclude = nullptr);

    /// Trigger / overlap queries: every active object inside an axis-aligned box or a sphere, each listed once.
    /// With no tag, an object counts when its mesh bounds overlap the volume. With a tag, only objects carrying
    /// that tag count, and one does when its pivot is inside or any mesh in its hierarchy overlaps - so a
    /// character whose meshes sit on child objects is reported as the tagged root.
    static std::vector<GameObject*> OverlapBox(Vec3 center, Vec3 halfExtents, const std::string& tag = "");
    static std::vector<GameObject*> OverlapSphere(Vec3 center, float radius, const std::string& tag = "");
};

} // namespace Phoenix
