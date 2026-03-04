#pragma once
#include "ResourceBase.h"
#include "Model.h"
#include <memory>

class ResourceMesh : public ResourceBase
{
public:
    ResourceMesh(UID uid);
    ~ResourceMesh() override;

    bool LoadInMemory()       override;
    void UnloadFromMemory()   override;

    Model* GetModel() const { return m_model.get(); }

private:
    std::unique_ptr<Model> m_model;
};