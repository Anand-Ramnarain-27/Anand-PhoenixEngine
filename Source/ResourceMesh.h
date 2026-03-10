#pragma once

#include "ResourceCommon.h"
#include "Mesh.h"
#include <memory>
#include <d3d12.h>

class ModuleStaticBuffer;

class ResourceMesh : public ResourceBase
{
public:
    explicit ResourceMesh(UID uid);
    ~ResourceMesh() override;

    bool LoadInMemory() override;
    bool LoadInMemory(ID3D12GraphicsCommandList* cmd, ModuleStaticBuffer* staticBuffer);

    void UnloadFromMemory() override;

    Mesh* getMesh() const { return m_mesh.get(); }

private:
    std::unique_ptr<Mesh> m_mesh;
};