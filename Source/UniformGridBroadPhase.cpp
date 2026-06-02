#include "Globals.h"
#include "UniformGridBroadPhase.h"

#include <algorithm>
#include <unordered_set>
#include <cmath>

// ---------------------------------------------------------------------------
// Cell-key encoding
//
// We pack (ix, iy, iz) into a single uint64_t using 21 bits per axis.
// kOffset centres the range so negative cell coordinates are representable.
// At the default cell size of 4 units this covers ±4 million world units per
// axis — far beyond any practical scene.
// ---------------------------------------------------------------------------
static constexpr int      kBits   = 21;
static constexpr int      kOffset = 1 << (kBits - 1); // 1 048 576
static constexpr uint64_t kMask   = (1ull << kBits) - 1;

static inline uint64_t packCell(int ix, int iy, int iz) {
    return (((uint64_t)(ix + kOffset) & kMask) << (kBits * 2))
         | (((uint64_t)(iy + kOffset) & kMask) <<  kBits)
         | (((uint64_t)(iz + kOffset) & kMask));
}

static inline void unpackCell(uint64_t key, int& ix, int& iy, int& iz) {
    ix = (int)((key >> (kBits * 2)) & kMask) - kOffset;
    iy = (int)((key >>  kBits)      & kMask) - kOffset;
    iz = (int)( key                  & kMask) - kOffset;
}

// ---------------------------------------------------------------------------

UniformGridBroadPhase::UniformGridBroadPhase(float cellSize)
    : m_cellSize(cellSize > 0.f ? cellSize : 4.f) {}

std::vector<CollisionPair> UniformGridBroadPhase::query(
    const std::vector<CollisionBody>& bodies)
{
    m_debugCells.clear();
    std::vector<CollisionPair> pairs;

    const uint32_t n = static_cast<uint32_t>(bodies.size());
    if (n < 2) return pairs;

    const float inv = 1.f / m_cellSize;

    // ---- Step 1: build flat (cellKey, bodyIndex) list ---------------------
    struct Entry { uint64_t key; uint32_t idx; };
    std::vector<Entry> flat;
    flat.reserve(n * 4); // most objects touch ≤4 cells

    for (uint32_t i = 0; i < n; ++i) {
        const AABB& box = bodies[i].worldAABB;

        // Integer cell range this AABB spans.
        int ixMn = (int)std::floor(box.min.x * inv);
        int iyMn = (int)std::floor(box.min.y * inv);
        int izMn = (int)std::floor(box.min.z * inv);
        int ixMx = (int)std::floor(box.max.x * inv);
        int iyMx = (int)std::floor(box.max.y * inv);
        int izMx = (int)std::floor(box.max.z * inv);

        for (int ix = ixMn; ix <= ixMx; ++ix)
            for (int iy = iyMn; iy <= iyMx; ++iy)
                for (int iz = izMn; iz <= izMx; ++iz)
                    flat.push_back({ packCell(ix, iy, iz), i });
    }

    // ---- Step 2: sort by cell key so same-cell entries are contiguous -----
    std::sort(flat.begin(), flat.end(),
              [](const Entry& a, const Entry& b){ return a.key < b.key; });

    // ---- Step 3: iterate cell groups, emit deduplicated pairs -------------
    // Pack (a,b) with a<b into uint64 for O(1) dedup lookup.
    std::unordered_set<uint64_t> seen;
    seen.reserve(n * 2);

    size_t i = 0;
    while (i < flat.size()) {
        uint64_t cellKey = flat[i].key;

        // Find end of this cell group.
        size_t j = i;
        while (j < flat.size() && flat[j].key == cellKey) ++j;
        // Group is flat[i .. j)

        // Generate all pairs within the group.
        for (size_t p = i; p < j; ++p) {
            for (size_t q = p + 1; q < j; ++q) {
                uint32_t a = flat[p].idx;
                uint32_t b = flat[q].idx;
                if (a > b) std::swap(a, b); // canonical: a < b
                uint64_t pairKey = ((uint64_t)a << 32) | b;
                if (seen.insert(pairKey).second)
                    pairs.push_back({ a, b });
            }
        }

        // Record this cell for debug draw.
        int ix, iy, iz;
        unpackCell(cellKey, ix, iy, iz);
        Vector3 cellMin(ix * m_cellSize, iy * m_cellSize, iz * m_cellSize);
        m_debugCells.push_back({ cellMin, cellMin + Vector3(m_cellSize, m_cellSize, m_cellSize) });

        i = j;
    }

    return pairs;
}

void UniformGridBroadPhase::drawDebug() {
    for (const auto& cell : m_debugCells)
        dd::aabb(ddConvert(cell.min), ddConvert(cell.max), dd::colors::Cyan);
}
