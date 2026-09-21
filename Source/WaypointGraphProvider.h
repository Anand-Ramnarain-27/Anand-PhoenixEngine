#pragma once
#include "INavProvider.h"
#include <string>
#include <vector>

// Hand-authored waypoint graph + A*.
class WaypointGraphProvider : public INavProvider {
public:
    struct Node {
        int id = -1;
        Vector3 position;
        std::vector<int> neighborIds;
    };

    bool FindPath(const Vector3& start, const Vector3& end,
                  const AgentProfile& profile, std::vector<Vector3>& outPath) override;
    bool IsWalkable(const Vector3& point, const AgentProfile& profile) const override;
    const char* getName() const override { return "Waypoint Graph"; }

    bool Load(const std::string& path);

    void drawDebug() const;

    const std::vector<Node>& getNodes() const { return nodes; }
    int getNodeCount() const { return (int)nodes.size(); }
    int getEdgeCount() const;

private:
    int findNearestNode(const Vector3& point) const;

    std::vector<Node> nodes;
};
