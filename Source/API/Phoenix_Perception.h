#pragma once
#include "API/Phoenix_Types.h"
#include <string>

class GameObject;

namespace Phoenix {

struct RaycastResult {
    bool hit = false;
    Vec3 point;
    Vec3 normal;
    float distance = 0.f;
    GameObject* object = nullptr;
};

struct Perception {
    static RaycastResult Raycast(Vec3 origin, Vec3 direction, float maxDistance);
    static bool IsLineClear(Vec3 from, Vec3 to);

    // Clear straight run of at least minLength (up to maxLength) from start.
    static bool ValidateChargeLane(Vec3 start, Vec3 direction, float minLength, float maxLength);

    static GameObject* FindNearestWithTag(const std::string& tag, Vec3 fromPosition, GameObject* exclude = nullptr);
};

} // namespace Phoenix
