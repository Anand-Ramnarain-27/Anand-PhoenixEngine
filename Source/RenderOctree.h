#pragma once
#include "BoundingVolume.h"
#include "Frustum.h"
#include <vector>
#include <memory>

class GameObject;

class RenderOctree {
public:
    struct Entry {
        GameObject* go;
        AABB worldAABB;
    };

    static void notifyTransformChanged(){ s_dirty = true; }

    void clear();
    void add(GameObject* go, const AABB& worldAABB);

    void build();

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
