#pragma once
#include "IBroadPhase.h"
#include "BoundingVolume.h"
#include <vector>

class UniformGridBroadPhase : public IBroadPhase {
public:
    explicit UniformGridBroadPhase(float cellSize = 4.f);

    std::vector<CollisionPair> query(
        const std::vector<CollisionBody>& bodies) override;

    void drawDebug() override;

    const char* getName() const override { return "Uniform Grid"; }

    float getCellSize() const { return m_cellSize; }
    void setCellSize(float s){ m_cellSize = (s > 0.f ? s : 0.1f); }

    int getLastCellCount() const { return static_cast<int>(m_debugCells.size()); }

private:
    float m_cellSize;
    std::vector<AABB> m_debugCells;
};
