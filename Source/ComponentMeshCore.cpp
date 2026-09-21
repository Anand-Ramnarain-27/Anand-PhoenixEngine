// AABB-only half of ComponentMesh, split out so PhoenixCore/GameScript.dll
// can compute mesh bounds without ComponentMesh.cpp's GPU-resource code.
#include "Globals.h"
#include "ComponentMesh.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "Mesh.h"
#include "Model.h"
#include "ResourceMesh.h"
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
