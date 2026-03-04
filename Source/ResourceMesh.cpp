#include "Globals.h"
#include "ResourceMesh.h"
#include "Model.h"

ResourceMesh::ResourceMesh(UID uid)
    : ResourceBase(uid, Type::Model)
{
}

ResourceMesh::~ResourceMesh()
{
    UnloadFromMemory();
}

bool ResourceMesh::LoadInMemory()
{
    if (m_model) return true; 

    auto model = std::make_unique<Model>();
    if (!model->load(libraryFile.c_str()))
    {
        LOG("ResourceMesh: Failed to load model from %s", libraryFile.c_str());
        return false;
    }

    m_model = std::move(model);
    return true;
}

void ResourceMesh::UnloadFromMemory()
{
    m_model.reset();
}