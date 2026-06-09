#pragma once
#include "IBroadPhase.h"
#include "BoundingVolume.h"
#include <vector>

// Uniform-grid broad phase.
// Each frame the grid is rebuilt from scratch:
//   1. Every body maps its worldAABB to the set of grid cells it overlaps.
//   2. A flat (cellKey, bodyIndex) list is sorted by cellKey.
//   3. Within each cell group, all body pairs are candidate pairs.
//   4. A seen-set (packed uint64 key) deduplicates pairs that appear in
//      multiple cells (e.g. a large object spanning 4 cells).
// Cell size is tunable at runtime.  A value of roughly 2× the average object
// diameter keeps most objects in 1–2 cells with minimal overflow.
// Replace by calling CollisionSystem::useGridBroadPhase(cellSize).
class UniformGridBroadPhase : public IBroadPhase {
public:
    explicit UniformGridBroadPhase(float cellSize = 4.f);

    std::vector<CollisionPair> query(
        const std::vector<CollisionBody>& bodies) override;

    // Draws the occupied cells as cyan wireframe boxes via dd::aabb.
    // Only draws cells that contained at least one body in the last query.
    void drawDebug() override;

    const char* getName() const override { return "Uniform Grid"; }

    float getCellSize() const { return m_cellSize; }
    void setCellSize(float s) { m_cellSize = (s > 0.f ? s : 0.1f); }

    // Number of distinct occupied cells in the last query — shown in the UI.
    int getLastCellCount() const { return static_cast<int>(m_debugCells.size()); }

private:
    float m_cellSize;
    std::vector<AABB> m_debugCells; // rebuilt each query(), used by drawDebug()
};
