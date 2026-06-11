#pragma once
#include "BoundingVolume.h"
#include "Frustum.h"
#include <vector>
#include <memory>

class GameObject;

// Sparse octree used for hierarchical render (frustum) culling — separate from
// OctreeBroadPhase (which is rebuilt every physics step for collision pairs).
//
// Usage (once per frame, in ModuleEditor::preRender):
//   octree.clear();
//   for each renderable GameObject: octree.add(go, worldAABB);
//   octree.build();                       // no-op unless something moved
//   octree.query(gameFrustum, visible);   // visible = candidate GameObjects
//
// Lazy rebuild: ComponentTransform::markDirty() calls notifyTransformChanged()
// whenever any GameObject's world transform changes. build() only reconstructs
// the tree if a transform changed (or the entry count changed) since the last
// build — a static scene reuses the previous frame's tree.
class RenderOctree {
public:
    struct Entry {
        GameObject* go;
        AABB worldAABB;
    };

    // Called by ComponentTransform::markDirty() — flags the tree as stale so
    // the next build() reconstructs it.
    static void notifyTransformChanged() { s_dirty = true; }

    void clear();
    void add(GameObject* go, const AABB& worldAABB);

    // Rebuild the tree from entries collected via add() since the last clear().
    // Skipped if neither the entry count nor a transform changed since the
    // last successful build (lazy rebuild).
    void build();

    // Append GameObjects whose world AABB overlaps the frustum.
    void query(const Frustum& frustum, std::vector<GameObject*>& outVisible) const;

    int getNodeCount() const { return m_nodeCount; }
    int getLeafCount() const { return m_leafCount; }

private:
    struct Node {
        AABB region;
        int depth = 0;
        std::vector<uint32_t> entryIndices;
        std::unique_ptr<Node> children[8];

        bool isLeaf() const { return !children[0]; }
        void subdivide(const std::vector<Entry>& entries, int capacity, int maxDepth);
        void insert(uint32_t idx, const AABB& bounds, const std::vector<Entry>& entries, int capacity, int maxDepth);
        void queryFrustum(const Frustum& frustum, const std::vector<Entry>& entries, std::vector<GameObject*>& out) const;
        void countStats(int& nodeCount, int& leafCount) const;
    };

    static AABB octant(const AABB& parent, int idx);

    std::vector<Entry> m_pending;
    std::unique_ptr<Node> m_root;
    size_t m_lastBuiltCount = SIZE_MAX;
    int m_nodeCount = 0;
    int m_leafCount = 0;

    static constexpr int NODE_CAPACITY = 8;
    static constexpr int MAX_DEPTH = 6;

    static bool s_dirty;
};
