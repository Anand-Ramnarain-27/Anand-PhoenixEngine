#include "Globals.h"
#include "ResourceMesh.h"
#include "MeshImporter.h"
#include "ModuleStaticBuffer.h"

ResourceMesh::ResourceMesh(UID uid) : ResourceBase(uid, Type::Mesh) {}
ResourceMesh::~ResourceMesh() { UnloadFromMemory(); }

bool ResourceMesh::LoadInMemory()
{
    if (m_mesh) return true;

    std::unique_ptr<Mesh> mesh;
    if (!MeshImporter::Load(libraryFile, mesh))
    {
        LOG("ResourceMesh: Failed to load %s", libraryFile.c_str());
        return false;
    }

    m_mesh = std::move(mesh);
    return true;
}

bool ResourceMesh::LoadInMemory(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer){
    if (m_mesh) return true;

    std::unique_ptr<Mesh> mesh;
    if (!MeshImporter::Load(libraryFile, cmd, staticBuffer, mesh)){
        LOG("ResourceMesh: Failed to load %s into static buffer", libraryFile.c_str());
        return false;
    }

    m_mesh = std::move(mesh);
    return true;
}

void ResourceMesh::UnloadFromMemory()
{
    m_mesh.reset();
}