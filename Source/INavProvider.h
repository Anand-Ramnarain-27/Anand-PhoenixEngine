#pragma once
#include "Globals.h"
#include <vector>

// Not used for clearance checks yet - added now so the interface below doesn't
// need to change signature once a geometry-aware provider needs it.
struct AgentProfile {
    float radius = 0.5f;
    float height = 1.8f;
    bool isMounted = false;
};

// Swappable path-source backend, mirrors IBroadPhase's role for collision.
class INavProvider {
public:
    virtual ~INavProvider() = default;

    // Appends waypoints to outPath. Returns false if no path was found.
    virtual bool FindPath(const Vector3& start, const Vector3& end,
                          const AgentProfile& profile, std::vector<Vector3>& outPath) = 0;

    virtual bool IsWalkable(const Vector3& point, const AgentProfile& profile) const = 0;

    virtual const char* getName() const = 0;
};
