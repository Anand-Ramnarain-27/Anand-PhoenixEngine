#include "Globals.h"
#include "CollisionSystem.h"
#include "BruteForceBroadPhase.h"
#include "UniformGridBroadPhase.h"
#include "OctreeBroadPhase.h"
#include "CollisionInterfaces.h"
#include <chrono>
#include "SceneGraph.h"
#include "GameObject.h"
#include <cfloat>
#include <cmath>

CollisionSystem::CollisionSystem()
    : m_broadPhase(std::make_unique<BruteForceBroadPhase>())
    , m_midPhase(std::make_unique<PassthroughMidPhase>())
{}

void CollisionSystem::setBroadPhase(std::unique_ptr<IBroadPhase> bp){
    if (bp) m_broadPhase = std::move(bp);
}

void CollisionSystem::useGridBroadPhase(float cellSize){
    m_broadPhase = std::make_unique<UniformGridBroadPhase>(cellSize);
}

void CollisionSystem::useBruteForceBroadPhase(){
    m_broadPhase = std::make_unique<BruteForceBroadPhase>();
}

bool CollisionSystem::isUsingGrid() const{
    return dynamic_cast<UniformGridBroadPhase*>(m_broadPhase.get()) != nullptr;
}

const char* CollisionSystem::getBroadPhaseName() const{
    return m_broadPhase ? m_broadPhase->getName() : "(none)";
}

static UniformGridBroadPhase* asGrid(IBroadPhase* bp){
    return dynamic_cast<UniformGridBroadPhase*>(bp);
}

static OctreeBroadPhase* asOctree(IBroadPhase* bp){
    return dynamic_cast<OctreeBroadPhase*>(bp);
}

float CollisionSystem::getGridCellSize() const{
    auto* g = asGrid(m_broadPhase.get());
    return g ? g->getCellSize() : 0.f;
}

void CollisionSystem::setGridCellSize(float s){
    if (auto* g = asGrid(m_broadPhase.get())) g->setCellSize(s);
}

int CollisionSystem::getLastGridCellCount() const{
    auto* g = asGrid(m_broadPhase.get());
    return g ? g->getLastCellCount() : 0;
}

void CollisionSystem::drawBroadPhaseDebug(){
    if (m_broadPhase) m_broadPhase->drawDebug();
}

void CollisionSystem::useOctreeBroadPhase(int nodeCapacity, int maxDepth){
    m_broadPhase = std::make_unique<OctreeBroadPhase>(nodeCapacity, maxDepth);
}

bool CollisionSystem::isUsingOctree() const{
    return asOctree(m_broadPhase.get()) != nullptr;
}

int CollisionSystem::getOctreeNodeCapacity() const{
    auto* o = asOctree(m_broadPhase.get());
    return o ? o->getNodeCapacity() : 0;
}

void CollisionSystem::setOctreeNodeCapacity(int c){
    if (auto* o = asOctree(m_broadPhase.get())) o->setNodeCapacity(c);
}

int CollisionSystem::getOctreeMaxDepth() const{
    auto* o = asOctree(m_broadPhase.get());
    return o ? o->getMaxDepth() : 0;
}

void CollisionSystem::setOctreeMaxDepth(int d){
    if (auto* o = asOctree(m_broadPhase.get())) o->setMaxDepth(d);
}

int CollisionSystem::getLastOctreeNodeCount() const{
    auto* o = asOctree(m_broadPhase.get());
    return o ? o->getLastNodeCount() : 0;
}

int CollisionSystem::getLastOctreeLeafCount() const{
    auto* o = asOctree(m_broadPhase.get());
    return o ? o->getLastLeafCount() : 0;
}

// buildOBB/applyBVType/gatherBodies/Raycast/IsLineClear are in CollisionSystemCore.cpp.

void CollisionSystem::run(SceneGraph* scene, float dt){
    m_results = {};

    std::vector<CollisionBody> bodies = gatherBodies(scene, dt);
    if (bodies.size() < 2) return;

    auto bpT0 = std::chrono::high_resolution_clock::now();
    auto broadPairs = m_broadPhase->query(bodies);
    auto bpT1 = std::chrono::high_resolution_clock::now();
    m_results.broadPhaseMs = std::chrono::duration<float, std::milli>(bpT1 - bpT0).count();
    m_results.broadCount = static_cast<uint32_t>(broadPairs.size());

    auto midPairs = m_midPhase->filter(std::move(broadPairs), bodies);
    m_results.midCount = static_cast<uint32_t>(midPairs.size());

    m_results.contacts = m_narrowPhase.test(midPairs, bodies);
    m_results.narrowCount = static_cast<uint32_t>(m_results.contacts.size());
}
