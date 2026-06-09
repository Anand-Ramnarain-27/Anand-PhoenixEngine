#pragma once
#include "CollisionTypes.h"
#include "IBroadPhase.h"
#include "IMidPhase.h"
#include "NarrowPhase.h"
#include <memory>

class ModuleScene;

// Owns and sequences the three pipeline stages each frame.
// Call setBroadPhase() to hot-swap the broad-phase implementation without
// touching MidPhase or NarrowPhase.
class CollisionSystem {
public:
    CollisionSystem();
    ~CollisionSystem() = default;

    // ---- Broad-phase swap helpers -----------------------------------------
    // Low-level: replace any implementation directly.
    void setBroadPhase(std::unique_ptr<IBroadPhase> bp);

    // High-level convenience: swap to the uniform-grid implementation.
    // cellSize — world-unit size of one grid cell (default 4, tune to ~2× avg
    // object diameter for best performance).
    void useGridBroadPhase(float cellSize = 4.f);

    // High-level convenience: swap back to the O(N²) brute-force reference.
    void useBruteForceBroadPhase();

    bool isUsingGrid() const;
    const char* getBroadPhaseName() const;

    // Grid-specific accessors (no-ops / return 0 when grid is not active).
    float getGridCellSize() const;
    void setGridCellSize(float s);
    int getLastGridCellCount() const;

    // High-level convenience: swap to the octree implementation.
    // nodeCapacity — max bodies per leaf before subdivision (default 8).
    // maxDepth     — hard recursion limit (default 6 → max 8^6 nodes).
    void useOctreeBroadPhase(int nodeCapacity = 8, int maxDepth = 6);
    bool isUsingOctree() const;

    // Octree-specific accessors (no-ops when octree is not active).
    int getOctreeNodeCapacity() const;
    void setOctreeNodeCapacity(int c);
    int getOctreeMaxDepth() const;
    void setOctreeMaxDepth(int d);
    int getLastOctreeNodeCount() const;
    int getLastOctreeLeafCount() const;

    // Call from the editor's render loop (inside editorExtras) to let the
    // active broad-phase draw its spatial structure (grid cells, BVH nodes…).
    void drawBroadPhaseDebug();

    // ---- Pipeline run ------------------------------------------------------
    // Gather objects from the scene, run the full pipeline, cache results.
    // dt is this frame's delta-time in seconds.  It is used to compute swept
    // AABBs for fast-moving bodies so the broad phase spans the full path.
    void run(ModuleScene* scene, float dt);

    const CollisionResults& getResults() const { return m_results; }

private:
    static std::vector<CollisionBody> gatherBodies(ModuleScene* scene, float dt);

    std::unique_ptr<IBroadPhase> m_broadPhase;
    std::unique_ptr<IMidPhase> m_midPhase;
    NarrowPhase m_narrowPhase;
    CollisionResults m_results;
};
