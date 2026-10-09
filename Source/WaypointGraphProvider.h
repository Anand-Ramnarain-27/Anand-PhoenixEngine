#pragma once
#include "INavProvider.h"
#include <string>
#include <vector>

// Waypoint graph (hand-authored or generated) + A*. Edge cost = length x the target node's optional Cost.
class WaypointGraphProvider : public INavProvider {
public:
    struct Node {
        int id = -1;
        Vector3 position;
        std::vector<int> neighborIds;
        // Optional "Cost" in the graph JSON (default 1): multiplies the length of every edge *into* this node, so a
        // path avoids it unless the detour is longer (e.g. orcs routing around their own traps). Must be > 0.
        float cost = 1.f;
        // Set at runtime (Navigation::SetNodesBlocked, e.g. a closed gate): paths never route through it.
        bool blocked = false;
    };

    bool FindPath(const Vector3& start, const Vector3& end,
                  const AgentProfile& profile, std::vector<Vector3>& outPath) override;
    bool IsWalkable(const Vector3& point, const AgentProfile& profile) const override;
    const char* getName() const override { return "Waypoint Graph"; }

    bool Load(const std::string& path);

    /// Blocks (or unblocks) every node within `radius` of `center`; returns how many changed state.
    int SetNodesBlocked(const Vector3& center, float radius, bool blocked);

    void drawDebug() const;

    const std::vector<Node>& getNodes() const { return nodes; }
    int getNodeCount() const { return (int)nodes.size(); }
    int getEdgeCount() const;

private:
    int findNearestNode(const Vector3& point) const;

    std::vector<Node> nodes;
};
