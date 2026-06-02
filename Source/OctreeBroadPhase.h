#pragma once
#include "IBroadPhase.h"
#include "BoundingVolume.h"
#include <vector>

// Octree broad phase — 3D spatial tree rebuilt from scratch each frame.
//
// Algorithm:
//   1. Compute the root AABB from the union of all body AABBs.
//   2. Insert every body.  A body is placed in every leaf node whose AABB
//      overlaps the body's worldAABB (multi-leaf insertion).
//   3. A leaf subdivides into 8 children when its occupancy exceeds nodeCapacity
//      AND its depth is below maxDepth.
//   4. Candidate pairs come from all (body_i, body_j) pairs that share a leaf.
//      A uint64 seen-set (same dedup strategy as UniformGridBroadPhase) removes
//      duplicates produced when a large body spans multiple leaves.
//
// Debug draw:
//   Root extent  — cyan wireframe box.
//   Non-empty leaf nodes — yellow wireframe boxes.
//   Clustered regions subdivide more finely; empty regions stay coarse.
class OctreeBroadPhase : public IBroadPhase {
public:
    // nodeCapacity — max bodies a leaf holds before splitting (default 8).
    // maxDepth     — hard depth limit to prevent runaway recursion (default 6,
    //                giving at most 8^6 = 262 144 leaf nodes).
    explicit OctreeBroadPhase(int nodeCapacity = 8, int maxDepth = 6);

    std::vector<CollisionPair> query(
        const std::vector<CollisionBody>& bodies) override;

    void drawDebug() override;

    const char* getName() const override { return "Octree"; }

    int  getNodeCapacity()   const { return m_nodeCapacity; }
    int  getMaxDepth()       const { return m_maxDepth; }
    void setNodeCapacity(int c)    { m_nodeCapacity = (c > 0 ? c : 1); }
    void setMaxDepth(int d)        { m_maxDepth = (d > 0 ? (d < 12 ? d : 12) : 1); }

    // Stats from the most recent query() — shown in the Collision Debug panel.
    int  getLastNodeCount() const { return m_lastNodeCount; }
    int  getLastLeafCount() const { return m_lastLeafCount; }

private:
    int m_nodeCapacity;
    int m_maxDepth;

    // Rebuilt each query().
    AABB              m_debugRoot;
    std::vector<AABB> m_debugLeaves; // non-empty leaves
    int               m_lastNodeCount = 0;
    int               m_lastLeafCount = 0;
};
