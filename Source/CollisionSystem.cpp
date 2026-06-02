#include "Globals.h"
#include "CollisionSystem.h"
#include "BruteForceBroadPhase.h"
#include "PassthroughMidPhase.h"
#include "ModuleScene.h"
#include "GameObject.h"
#include "ComponentMesh.h"
#include "ComponentTransform.h"
#include <functional>
#include <cfloat>

CollisionSystem::CollisionSystem()
    : m_broadPhase(std::make_unique<BruteForceBroadPhase>())
    , m_midPhase(std::make_unique<PassthroughMidPhase>())
{}

void CollisionSystem::setBroadPhase(std::unique_ptr<IBroadPhase> bp) {
    if (bp) m_broadPhase = std::move(bp);
}

// ---------------------------------------------------------------------------
// Build an OBB for a CollisionBody from the world matrix.
// Extracts scale from each row's length, then normalises to get pure axes.
// ---------------------------------------------------------------------------
static void buildOBB(CollisionBody& body) {
    const ComponentTransform* t = body.go->getTransform();
    const ComponentMesh*      cm = body.go->getComponent<ComponentMesh>();
    if (!t || !cm || !cm->hasAABB()) return;

    const Matrix& W      = const_cast<ComponentTransform*>(t)->getGlobalMatrix();
    const Vector3 lMin   = cm->getLocalAABBMin();
    const Vector3 lMax   = cm->getLocalAABBMax();
    const Vector3 lHalf  = (lMax - lMin) * 0.5f;
    const Vector3 lCtr   = (lMin + lMax) * 0.5f;

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

std::vector<CollisionBody> CollisionSystem::gatherBodies(ModuleScene* scene) {
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
            bodies.push_back(body);
        }
        for (auto* child : node->getChildren()) visit(child);
    };
    visit(scene->getRoot());
    return bodies;
}

void CollisionSystem::run(ModuleScene* scene) {
    m_results = {};

    std::vector<CollisionBody> bodies = gatherBodies(scene);
    if (bodies.size() < 2) return;

    // --- Broad phase ---
    auto broadPairs = m_broadPhase->query(bodies);
    m_results.broadCount = static_cast<uint32_t>(broadPairs.size());

    // --- Mid phase ---
    auto midPairs = m_midPhase->filter(std::move(broadPairs), bodies);
    m_results.midCount = static_cast<uint32_t>(midPairs.size());

    // --- Narrow phase ---
    m_results.contacts    = m_narrowPhase.test(midPairs, bodies);
    m_results.narrowCount = static_cast<uint32_t>(m_results.contacts.size());
}
