#include "Globals.h"
#include "ResourceMesh.h"
#include "MeshImporter.h"
#include "Mesh.h"
#include "ModuleStaticBuffer.h"
#include <cstdint>

ResourceMesh::ResourceMesh(UID uid) : ResourceBase(uid, Type::Mesh) {}
ResourceMesh::~ResourceMesh() { UnloadFromMemory(); }

bool ResourceMesh::LoadInMemory() {
    if (m_mesh) return true;
    std::unique_ptr<Mesh> mesh;
    if (!MeshImporter::Load(libraryFile, mesh)) { LOG("ResourceMesh: Failed to load %s", libraryFile.c_str()); return false; }
    m_mesh = std::move(mesh);
    return true;
}

bool ResourceMesh::LoadInMemory(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer) {
    if (m_mesh) {
        m_mesh->uploadToGPU(cmd, staticBuffer);
        return true;
    }
    std::unique_ptr<Mesh> mesh;
    if (!MeshImporter::Load(libraryFile, cmd, staticBuffer, mesh)) { LOG("ResourceMesh: Failed to load %s into static buffer", libraryFile.c_str()); return false; }
    m_mesh = std::move(mesh);
    return true;
}

bool ResourceMesh::isOnGPU() const {
    return m_mesh && m_mesh->isOnGPU();
}

uint32_t ResourceMesh::getNumMorphTargets() const {
    return m_mesh ? m_mesh->getNumMorphTargets() : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS ResourceMesh::getMorphTargetBufferVA() const {
    return m_mesh ? m_mesh->getMorphTargetBufferVA() : 0;
}

void ResourceMesh::UnloadFromMemory() {
    m_mesh.reset();
}