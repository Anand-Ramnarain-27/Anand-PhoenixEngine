#pragma once
#include "ResourceBase.h"
#include "Material.h"
#include <memory>

class ResourceMaterial : public ResourceBase
{
public:
    ResourceMaterial(UID uid);
    ~ResourceMaterial() override;

    bool LoadInMemory()     override;
    void UnloadFromMemory() override;

    Material* getMaterial() { return m_material.get(); }
    const Material* getMaterial() const { return m_material.get(); }

    UID textureUID = 0;

private:
    std::unique_ptr<Material> m_material;
};