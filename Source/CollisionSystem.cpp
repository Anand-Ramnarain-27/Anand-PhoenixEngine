#include "Globals.h"
#include "CollisionSystem.h"
#include "BruteForceBroadPhase.h"
#include "UniformGridBroadPhase.h"
#include "OctreeBroadPhase.h"
#include "IMidPhase.h"
#include <chrono>
#include "ModuleScene.h"
#include "GameObject.h"
#include "ComponentMesh.h"
#include "ComponentTransform.h"
#include "ComponentBounds.h"
#include "ComponentRigidbody.h"
#include <functional>
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

// ---------------------------------------------------------------------------
// Build an OBB for a CollisionBody from the world matrix.
// Extracts scale from each row's length, then normalises to get pure axes.
// ---------------------------------------------------------------------------
static void buildOBB(CollisionBody& body){
    const ComponentTransform* t = body.go->getTransform();
    const ComponentMesh* cm = body.go->getComponent<ComponentMesh>();
    if (!t || !cm || !cm->hasAABB()) return;

    const Matrix& W = const_cast<ComponentTransform*>(t)->getGlobalMatrix();
    const Vector3 lMin = cm->getLocalAABBMin();
    const Vector3 lMax = cm->getLocalAABBMax();
    const Vector3 lHalf = (lMax - lMin) * 0.5f;
    const Vector3 lCtr = (lMin + lMax) * 0.5f;

    body.obbCenter = Vector3::Transform(lCtr, W);

    // Row 0..2 of the world matrix are the scaled world-space axes.
    Vector3 cx(W._11, W._12, W._13);
    Vector3 cy(W._21, W._22, W._23);
    Vector3 cz(W._31, W._32, W._33);

    float sx = cx.Length(), sy = cy.Length(), sz = cz.Length();
    static const float kEps = 1e-8f;
    body.obbAxes[0] = sx > kEps ? cx / sx : Vector3::UnitX;
    body.obbAxes[1] = sy > kEps ? cy / sy : Vector3::UnitY;
    body.obbAxes[2] = sz > kEps ? cz / sz : Vector3::UnitZ;
    body.obbHalves[0] = lHalf.x * sx;
    body.obbHalves[1] = lHalf.y * sy;
    body.obbHalves[2] = lHalf.z * sz;
}

// ---------------------------------------------------------------------------
// Choose the bounding volume type for a body and populate its sphere fields.
// Must be called after buildOBB() so obbCenter/obbHalves are valid.
// ---------------------------------------------------------------------------
static void applyBVType(CollisionBody& body){
    const ComponentBounds* cb = body.go->getComponent<ComponentBounds>();
    if (!cb || cb->bvType == BVType::AABB) {
        body.bvType = BVType::AABB;
        // worldAABB already set from the mesh AABB — nothing more to do.
        return;
    }

    body.bvType = BVType::Sphere;
    body.sphereCenter = body.obbCenter;

    if (cb->radiusOverride >= 0.f) {
        body.sphereRadius = cb->radiusOverride;
    } else {
        // Circumscribed sphere: radius = half-diagonal of the OBB.
        body.sphereRadius = sqrtf(
            body.obbHalves[0] * body.obbHalves[0] +
            body.obbHalves[1] * body.obbHalves[1] +
            body.obbHalves[2] * body.obbHalves[2]);
    }

    // Replace the broad-phase proxy with the sphere's tight enclosing AABB.
    Sphere s{ body.sphereCenter, body.sphereRadius };
    body.worldAABB = s.toAABB();
}

// ---------------------------------------------------------------------------

std::vector<CollisionBody> CollisionSystem::gatherBodies(ModuleScene* scene, float dt){
    std::vector<CollisionBody> bodies;
    if (!scene) return bodies;

    std::function<void(GameObject*)> visit = [&](GameObject* node) {
        if (!node || !node->isActive()) return;
        ComponentMesh* cm = node->getComponent<ComponentMesh>();
        if (cm && cm->hasAABB()) {
            CollisionBody body;
            body.go = node;
            Vector3 mn, mx;
            cm->getWorldAABB(mn, mx);
            body.worldAABB.min = mn;
            body.worldAABB.max = mx;
            buildOBB(body);
            applyBVType(body); // overrides worldAABB + sets sphere fields if needed

            // ---- Swept AABB for fast-moving objects ----
            // Expand the broad-phase proxy to also cover the AABB at the
            // predicted next-frame position (current + velocity * dt).  This
            // guarantees the broad phase emits a candidate pair even when the
            // object is not yet overlapping the obstacle at its current
            // position but will be at the start of the next frame.
            // The narrow phase still uses the unmodified OBB/sphere.
            const ComponentRigidbody* rb = node->getComponent<ComponentRigidbody>();
            if (rb && rb->isFastMoving && !rb->isStatic && dt > 1e-7f) {
                const Vector3 disp = rb->velocity * dt;
                // Union of current AABB with AABB displaced by one frame.
                body.worldAABB.min = Vector3::Min(body.worldAABB.min,
                                                   body.worldAABB.min + disp);
                body.worldAABB.max = Vector3::Max(body.worldAABB.max,
                                                   body.worldAABB.max + disp);
            }

            bodies.push_back(body);
        }
        for (auto* child : node->getChildren()) visit(child);
    };
    visit(scene->getRoot());
    return bodies;
}

void CollisionSystem::run(ModuleScene* scene, float dt){
    m_results = {};

    std::vector<CollisionBody> bodies = gatherBodies(scene, dt);
    if (bodies.size() < 2) return;

    // --- Broad phase (timed) ---
    auto bpT0 = std::chrono::high_resolution_clock::now();
    auto broadPairs = m_broadPhase->query(bodies);
    auto bpT1 = std::chrono::high_resolution_clock::now();
    m_results.broadPhaseMs = std::chrono::duration<float, std::milli>(bpT1 - bpT0).count();
    m_results.broadCount = static_cast<uint32_t>(broadPairs.size());

    // --- Mid phase ---
    auto midPairs = m_midPhase->filter(std::move(broadPairs), bodies);
    m_results.midCount = static_cast<uint32_t>(midPairs.size());

    // --- Narrow phase ---
    m_results.contacts = m_narrowPhase.test(midPairs, bodies);
    m_results.narrowCount = static_cast<uint32_t>(m_results.contacts.size());
}
