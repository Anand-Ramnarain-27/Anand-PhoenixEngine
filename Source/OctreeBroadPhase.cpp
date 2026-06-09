#include "Globals.h"
#include "OctreeBroadPhase.h"

#include <algorithm>
#include <unordered_set>
#include <memory>
#include <cfloat>

// ---------------------------------------------------------------------------
// OctreeNode — internal implementation type, not exposed in the header.
// A node holds a list of body indices when it is a leaf.  Once the list
// exceeds nodeCapacity AND depth < maxDepth, subdivide() is called:
//   • 8 children are created (one per octant).
//   • All existing bodies are re-inserted into the children.
//   • The node's own list is cleared — it is now an internal node.
// Multi-leaf insertion: every body is inserted into every leaf node whose
// region overlaps the body's AABB.  Pair generation at the leaf level
// combined with a uint64 deduplication set ensures correctness without
// ever needing an "ancestor body" propagation pass.
// ---------------------------------------------------------------------------

namespace {

struct OctreeNode {
    AABB region;
    int depth = 0;
    std::vector<uint32_t> bodyIndices;
    std::unique_ptr<OctreeNode> children[8]; // null ⇒ leaf

    bool isLeaf() const { return !children[0]; }

    // Return the idx-th octant of parent.
    // Bit 0 → X, bit 1 → Y, bit 2 → Z.
    static AABB octant(const AABB& parent, int idx){
        const Vector3 mid = (parent.min + parent.max) * 0.5f;
        Vector3 lo, hi;
        lo.x = (idx & 1) ? mid.x : parent.min.x;
        hi.x = (idx & 1) ? parent.max.x : mid.x;
        lo.y = (idx & 2) ? mid.y : parent.min.y;
        hi.y = (idx & 2) ? parent.max.y : mid.y;
        lo.z = (idx & 4) ? mid.z : parent.min.z;
        hi.z = (idx & 4) ? parent.max.z : mid.z;
        return { lo, hi };
    }

    // Split this leaf into 8 children and re-distribute existing bodies.
    void subdivide(const std::vector<CollisionBody>& allBodies,
                   int capacity, int maxDepth){
        for (int i = 0; i < 8; ++i) {
            children[i] = std::make_unique<OctreeNode>();
            children[i]->region = octant(region, i);
            children[i]->depth = depth + 1;
        }
        for (uint32_t b : bodyIndices)
            for (auto& c : children)
                c->insert(b, allBodies[b].worldAABB, allBodies, capacity, maxDepth);
        bodyIndices.clear();
    }

    // Insert body idx.  Recursion terminates when the body's AABB doesn't
    // intersect this node's region.
    void insert(uint32_t idx, const AABB& bodyAABB,
                const std::vector<CollisionBody>& allBodies,
                int capacity, int maxDepth){
        if (!region.intersects(bodyAABB)) return;
        if (isLeaf()) {
            bodyIndices.push_back(idx);
            if (static_cast<int>(bodyIndices.size()) > capacity && depth < maxDepth)
                subdivide(allBodies, capacity, maxDepth);
        } else {
            for (auto& c : children)
                c->insert(idx, bodyAABB, allBodies, capacity, maxDepth);
        }
    }

    // Walk the tree, emitting deduplicated pairs from every leaf.
    void collectPairs(std::unordered_set<uint64_t>& seen,
                      std::vector<CollisionPair>& pairs,
                      std::vector<AABB>& debugLeaves,
                      int& nodeCount, int& leafCount) const{
        ++nodeCount;
        if (isLeaf()) {
            ++leafCount;
            const size_t n = bodyIndices.size();
            if (n > 0) debugLeaves.push_back(region);
            for (size_t i = 0; i < n; ++i) {
                for (size_t j = i + 1; j < n; ++j) {
                    uint32_t a = bodyIndices[i];
                    uint32_t b = bodyIndices[j];
                    if (a > b) std::swap(a, b);
                    uint64_t key = ((uint64_t)a << 32) | b;
                    if (seen.insert(key).second)
                        pairs.push_back({ a, b });
                }
            }
        } else {
            for (const auto& c : children)
                if (c) c->collectPairs(seen, pairs, debugLeaves, nodeCount, leafCount);
        }
    }
};

} // namespace

// ---------------------------------------------------------------------------

OctreeBroadPhase::OctreeBroadPhase(int nodeCapacity, int maxDepth)
    : m_nodeCapacity(nodeCapacity > 0 ? nodeCapacity : 8)
    , m_maxDepth(maxDepth > 0 ? (maxDepth < 12 ? maxDepth : 12) : 6)
{}

std::vector<CollisionPair> OctreeBroadPhase::query(
    const std::vector<CollisionBody>& bodies){
    m_debugLeaves.clear();
    m_lastNodeCount = 0;
    m_lastLeafCount = 0;
    m_debugRoot = {};

    std::vector<CollisionPair> pairs;
    const uint32_t n = static_cast<uint32_t>(bodies.size());
    if (n < 2) return pairs;

    // ---- Compute root region from union of all worldAABBs ----
    AABB root;
    root.min = Vector3( FLT_MAX, FLT_MAX, FLT_MAX);
    root.max = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const auto& b : bodies) {
        root.min = Vector3::Min(root.min, b.worldAABB.min);
        root.max = Vector3::Max(root.max, b.worldAABB.max);
    }
    // Expand by a small epsilon so objects on the exact boundary are handled.
    const Vector3 kEps(0.01f, 0.01f, 0.01f);
    root.min -= kEps;
    root.max += kEps;
    m_debugRoot = root;

    // ---- Build tree ----
    OctreeNode rootNode;
    rootNode.region = root;
    rootNode.depth = 0;
    for (uint32_t i = 0; i < n; ++i)
        rootNode.insert(i, bodies[i].worldAABB, bodies, m_nodeCapacity, m_maxDepth);

    // ---- Collect candidate pairs ----
    std::unordered_set<uint64_t> seen;
    seen.reserve(n * 4);
    rootNode.collectPairs(seen, pairs, m_debugLeaves, m_lastNodeCount, m_lastLeafCount);

    return pairs;
}

void OctreeBroadPhase::drawDebug(){
    // Root extent in cyan — shows the total world region covered.
    if (m_debugRoot.isValid())
        dd::aabb(ddConvert(m_debugRoot.min), ddConvert(m_debugRoot.max), dd::colors::Cyan);

    // Non-empty leaves in yellow — dense regions show many small boxes,
    // sparse regions show large boxes, empty regions show nothing.
    for (const auto& leaf : m_debugLeaves)
        dd::aabb(ddConvert(leaf.min), ddConvert(leaf.max), dd::colors::Yellow);
}
