#pragma once
#include "CollisionInterfaces.h"
#include "BoundingVolume.h"
#include <vector>

class OctreeBroadPhase : public IBroadPhase {
public:
    explicit OctreeBroadPhase(int nodeCapacity = 8, int maxDepth = 6);

    std::vector<CollisionPair> query(
        const std::vector<CollisionBody>& bodies) override;

    void drawDebug() override;

    const char* getName() const override { return "Octree"; }

    int getNodeCapacity() const { return m_nodeCapacity; }
    int getMaxDepth() const { return m_maxDepth; }
    void setNodeCapacity(int c){ m_nodeCapacity = (c > 0 ? c : 1); }
    void setMaxDepth(int d){ m_maxDepth = (d > 0 ? (d < 12 ? d : 12) : 1); }

    int getLastNodeCount() const { return m_lastNodeCount; }
    int getLastLeafCount() const { return m_lastLeafCount; }

private:
    int m_nodeCapacity;
    int m_maxDepth;

    AABB m_debugRoot;
    std::vector<AABB> m_debugLeaves;
    int m_lastNodeCount = 0;
    int m_lastLeafCount = 0;
};
