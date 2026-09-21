#pragma once
#include "API/Phoenix_Types.h"
#include <string>
#include <vector>

namespace Phoenix {

struct Navigation {
    // Uses the scene's active waypoint graph.
    static bool FindPath(Vec3 start, Vec3 end, std::vector<Vec3>& outPath);
    static bool IsWalkable(Vec3 point);

    // Additional named graphs alongside the active one (climb anchors, burrow islands, etc).
    static bool LoadGraph(const std::string& name, const std::string& path);
    static bool FindPathNamed(const std::string& name, Vec3 start, Vec3 end, std::vector<Vec3>& outPath);
    static bool IsWalkableNamed(const std::string& name, Vec3 point);
};

} // namespace Phoenix
