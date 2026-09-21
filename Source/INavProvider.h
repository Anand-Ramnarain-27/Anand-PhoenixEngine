#pragma once
#include "Globals.h"
#include <vector>

// Movement-capability tag for a path request - lets a provider reject/adjust
// routes based on who's asking (doorway-blocking size tiers, mounted turn
// radius, etc). Not yet used for real clearance checks by any provider - the
// point of adding it now is that the FindPath/IsWalkable signature never has
// to change again once a geometry-aware provider needs it.
struct AgentProfile {
    float radius = 0.5f;
    float height = 1.8f;
    bool isMounted = false;
};

// Swappable path-source backend (mirrors IBroadPhase's role for collision).
// ComponentAIAgent and player movement code talk only to this interface, never
// to a concrete backend - WaypointGraphProvider today, NavGridProvider /
// RecastNavMeshProvider later, with zero changes required above this line.
class INavProvider {
public:
    virtual ~INavProvider() = default;

    // Finds a path from start to end, appending waypoint positions to outPath.
    // Returns false if no path could be found (outPath is left unmodified).
    virtual bool FindPath(const Vector3& start, const Vector3& end,
                          const AgentProfile& profile, std::vector<Vector3>& outPath) = 0;

    // Returns true if the given point is considered walkable/on the nav data.
    virtual bool IsWalkable(const Vector3& point, const AgentProfile& profile) const = 0;

    virtual const char* getName() const = 0;
};
