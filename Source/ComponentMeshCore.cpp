// AABB-only half of ComponentMesh, split out so PhoenixCore/GameScript.dll
// can compute mesh bounds without ComponentMesh.cpp's GPU-resource code.
#include "Globals.h"
#include "ComponentMesh.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "Mesh.h"
#include "Model.h"
#include "ResourceMesh.h"
#include "RayMath.h"
#include <cfloat>

void ComponentMesh::computeLocalAABB(){
    m_localAABBMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    m_localAABBMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    m_hasAABB = false;
    for (const auto& e : m_entries){
        if (!e.meshRes || !e.meshRes->getMesh()) continue;
        const Mesh* mesh = e.meshRes->getMesh();
        if (!mesh->hasAABB()) continue;
        m_localAABBMin = Vector3::Min(m_localAABBMin, mesh->getAABBMin());
        m_localAABBMax = Vector3::Max(m_localAABBMax, mesh->getAABBMax());
        m_hasAABB = true;
    }
    if (m_proceduralModel){
        for (const auto& mesh : m_proceduralModel->getMeshes()){
            if (!mesh || !mesh->hasAABB()) continue;
            m_localAABBMin = Vector3::Min(m_localAABBMin, mesh->getAABBMin());
            m_localAABBMax = Vector3::Max(m_localAABBMax, mesh->getAABBMax());
            m_hasAABB = true;
        }
    }
}

namespace {
    bool meshHasTriangles(const Mesh* mesh){
        return mesh && !mesh->hasBoneWeights() && mesh->getIndexCount() >= 3 && !mesh->getVertices().empty();
    }

    template<typename Fn>
    void forEachMesh(const std::vector<MeshEntry>& entries, const std::shared_ptr<Model>& procedural, Fn&& fn){
        for (const auto& e : entries)
            if (e.meshRes) fn(e.meshRes->getMesh());
        if (procedural)
            for (const auto& mesh : procedural->getMeshes()) fn(mesh.get());
    }
}

bool ComponentMesh::hasRaycastTriangles() const{
    if (m_hasSkin) return false;
    bool any = false;
    forEachMesh(m_entries, m_proceduralModel, [&](const Mesh* mesh){ any = any || meshHasTriangles(mesh); });
    return any;
}

bool ComponentMesh::raycastTriangles(const Vector3& origin, const Vector3& dir, float maxDist,
                                     float& outDist, Vector3& outNormal) const{
    if (m_hasSkin) return false;
    auto* t = owner->getTransform();
    const Matrix world = t ? t->getGlobalMatrix() : Matrix::Identity;
    const Matrix inv = world.Invert();

    // Test in mesh space. The direction isn't renormalized, so ray parameters stay world distances.
    const RayMath::Ray local{ Vector3::Transform(origin, inv), Vector3::TransformNormal(dir, inv) };

    float best = maxDist;
    Vector3 bestA, bestB, bestC;
    bool hit = false;
    forEachMesh(m_entries, m_proceduralModel, [&](const Mesh* mesh){
        if (!meshHasTriangles(mesh)) return;
        if (mesh->hasAABB()){
            const Vector3& mn = mesh->getAABBMin();
            const Vector3& mx = mesh->getAABBMax();
            const Vector3& o = local.origin;
            const bool inside = o.x >= mn.x && o.x <= mx.x && o.y >= mn.y && o.y <= mx.y && o.z >= mn.z && o.z <= mx.z;
            if (!inside && RayMath::RayVsAABB(local, mn, mx, best) == FLT_MAX) return;
        }
        const auto& verts = mesh->getVertices();
        const auto& idx = mesh->getIndices();
        for (size_t i = 0; i + 2 < idx.size(); i += 3){
            if (idx[i] >= verts.size() || idx[i + 1] >= verts.size() || idx[i + 2] >= verts.size()) continue;
            const Vector3& a = verts[idx[i]].position;
            const Vector3& b = verts[idx[i + 1]].position;
            const Vector3& c = verts[idx[i + 2]].position;
            const float d = RayMath::RayVsTriangle(local, a, b, c);
            if (d < best){ best = d; bestA = a; bestB = b; bestC = c; hit = true; }
        }
    });
    if (!hit) return false;

    Vector3 n = Vector3::TransformNormal((bestB - bestA).Cross(bestC - bestA), inv.Transpose());
    n.Normalize();
    if (n.Dot(dir) > 0.f) n = -n;
    outDist = best;
    outNormal = n;
    return true;
}

void ComponentMesh::getWorldAABB(Vector3& outMin, Vector3& outMax) const{
    auto* t = owner->getTransform();
    Matrix world = t ? t->getGlobalMatrix() : Matrix::Identity;
    const Vector3& mn = m_localAABBMin;
    const Vector3& mx = m_localAABBMax;
    Vector3 corners[8] = {
        {mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mn.x,mx.y,mn.z},{mx.x,mx.y,mn.z},
        {mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mn.x,mx.y,mx.z},{mx.x,mx.y,mx.z},
    };
    outMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    outMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const auto& c : corners){
        Vector3 wc = Vector3::Transform(c, world);
        outMin = Vector3::Min(outMin, wc);
        outMax = Vector3::Max(outMax, wc);
    }
}
