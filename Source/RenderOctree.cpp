#include "Globals.h"
#include "RenderOctree.h"
#include <cfloat>

bool RenderOctree::s_dirty = true;

AABB RenderOctree::octant(const AABB& parent, int idx){
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

void RenderOctree::Node::subdivide(const std::vector<Entry>& entries, int capacity, int maxDepth){
    for (int i = 0; i < 8; ++i) {
        children[i] = std::make_unique<Node>();
        children[i]->region = octant(region, i);
        children[i]->depth = depth + 1;
    }
    for (uint32_t idx : entryIndices)
        for (auto& c : children)
            c->insert(idx, entries[idx].worldAABB, entries, capacity, maxDepth);
    entryIndices.clear();
}

void RenderOctree::Node::insert(uint32_t idx, const AABB& bounds, const std::vector<Entry>& entries, int capacity, int maxDepth){
    if (!region.intersects(bounds)) return;
    if (isLeaf()) {
        entryIndices.push_back(idx);
        if (static_cast<int>(entryIndices.size()) > capacity && depth < maxDepth)
            subdivide(entries, capacity, maxDepth);
    } else {
        for (auto& c : children)
            c->insert(idx, bounds, entries, capacity, maxDepth);
    }
}

void RenderOctree::Node::queryFrustum(const Frustum& frustum, const std::vector<Entry>& entries, std::vector<GameObject*>& out) const{
    if (!frustum.intersectsAABB(region.min, region.max)) return;
    if (isLeaf()) {
        for (uint32_t idx : entryIndices) out.push_back(entries[idx].go);
    } else {
        for (const auto& c : children)
            if (c) c->queryFrustum(frustum, entries, out);
    }
}

void RenderOctree::Node::countStats(int& nodeCount, int& leafCount) const{
    ++nodeCount;
    if (isLeaf()) ++leafCount;
    else for (const auto& c : children) if (c) c->countStats(nodeCount, leafCount);
}

void RenderOctree::clear(){
    m_pending.clear();
}

void RenderOctree::add(GameObject* go, const AABB& worldAABB){
    m_pending.push_back({ go, worldAABB });
}

void RenderOctree::build(){
    // Lazy rebuild: skip unless a transform changed or the renderable set
    // itself changed size (object added/removed/disabled) since last build.
    if (!s_dirty && m_pending.size() == m_lastBuiltCount) return;
    s_dirty = false;
    m_lastBuiltCount = m_pending.size();
    m_nodeCount = 0;
    m_leafCount = 0;
    m_root.reset();

    if (m_pending.empty()) return;

    AABB root;
    root.min = Vector3( FLT_MAX, FLT_MAX, FLT_MAX);
    root.max = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const auto& e : m_pending) {
        root.min = Vector3::Min(root.min, e.worldAABB.min);
        root.max = Vector3::Max(root.max, e.worldAABB.max);
    }
    const Vector3 kEps(0.01f, 0.01f, 0.01f);
    root.min -= kEps;
    root.max += kEps;

    m_root = std::make_unique<Node>();
    m_root->region = root;
    m_root->depth = 0;
    for (uint32_t i = 0; i < (uint32_t)m_pending.size(); ++i)
        m_root->insert(i, m_pending[i].worldAABB, m_pending, NODE_CAPACITY, MAX_DEPTH);

    m_root->countStats(m_nodeCount, m_leafCount);
}

void RenderOctree::query(const Frustum& frustum, std::vector<GameObject*>& outVisible) const{
    if (m_root) m_root->queryFrustum(frustum, m_pending, outVisible);
}
