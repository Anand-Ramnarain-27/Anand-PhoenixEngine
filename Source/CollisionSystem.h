#pragma once
#include "CollisionInterfaces.h"
#include "NarrowPhase.h"
#include <memory>

class SceneGraph;
class GameObject;

struct RaycastHit {
    bool hit = false;
    Vector3 point;
    Vector3 normal;
    float distance = 0.f;
    GameObject* object = nullptr;
};

class CollisionSystem {
public:
    CollisionSystem();
    ~CollisionSystem() = default;

    void setBroadPhase(std::unique_ptr<IBroadPhase> bp);

    void useGridBroadPhase(float cellSize = 4.f);

    void useBruteForceBroadPhase();

    bool isUsingGrid() const;
    const char* getBroadPhaseName() const;

    float getGridCellSize() const;
    void setGridCellSize(float s);
    int getLastGridCellCount() const;

    void useOctreeBroadPhase(int nodeCapacity = 8, int maxDepth = 6);
    bool isUsingOctree() const;

    int getOctreeNodeCapacity() const;
    void setOctreeNodeCapacity(int c);
    int getOctreeMaxDepth() const;
    void setOctreeMaxDepth(int d);
    int getLastOctreeNodeCount() const;
    int getLastOctreeLeafCount() const;

    void drawBroadPhaseDebug();

    void run(SceneGraph* scene, float dt);

    const CollisionResults& getResults() const { return m_results; }

    // Independent of run()/getResults() - gathers bodies fresh each call.
    static bool Raycast(SceneGraph* scene, const Vector3& origin, const Vector3& dir,
                        float maxDistance, RaycastHit& outHit);
    static bool IsLineClear(SceneGraph* scene, const Vector3& from, const Vector3& to);

private:
    static std::vector<CollisionBody> gatherBodies(SceneGraph* scene, float dt);

    std::unique_ptr<IBroadPhase> m_broadPhase;
    std::unique_ptr<IMidPhase> m_midPhase;
    NarrowPhase m_narrowPhase;
    CollisionResults m_results;
};
