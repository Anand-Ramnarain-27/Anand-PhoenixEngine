#pragma once
#include "API/Phoenix_Types.h"
#include <string>
#include <vector>

namespace Phoenix {

struct Navigation {
    // Uses the scene's single "active" waypoint graph (Assets/Navigation/*.json).
    static bool FindPath(Vec3 start, Vec3 end, std::vector<Vec3>& outPath);
    static bool IsWalkable(Vec3 point);

    // Named graphs coexisting alongside the active one - climb anchors, burrow
    // islands, reveal points, or anything else that isn't "the" ground mesh.
    static bool LoadGraph(const std::string& name, const std::string& path);
    static bool FindPathNamed(const std::string& name, Vec3 start, Vec3 end, std::vector<Vec3>& outPath);
    static bool IsWalkableNamed(const std::string& name, Vec3 point);
};

} // namespace Phoenix
