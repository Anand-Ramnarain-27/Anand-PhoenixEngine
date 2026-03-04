#pragma once
#include "ResourceBase.h"
#include "Mesh.h"
#include <memory>

class ResourceMesh : public ResourceBase
{
public:
    ResourceMesh(UID uid);
    ~ResourceMesh() override;

    bool LoadInMemory()     override;
    void UnloadFromMemory() override;

    Mesh* getMesh() const { return m_mesh.get(); }

private:
    std::unique_ptr<Mesh> m_mesh;
};