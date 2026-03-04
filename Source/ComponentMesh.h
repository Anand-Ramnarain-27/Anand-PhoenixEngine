#pragma once
#include "Component.h"
#include "Material.h"
#include "Model.h"
#include "ModuleD3D12.h"
#include "ResourceMesh.h"
#include <memory>
#include <vector>
#include <string>

class ComponentMesh : public Component
{
public:
    explicit ComponentMesh(GameObject* owner);
    ~ComponentMesh() override;

    bool loadModel(const char* filePath);

    void setModel(std::unique_ptr<Model> model);

    void rebuildMaterialBuffers();

    void render(ID3D12GraphicsCommandList* cmd) override;
    void onSave(std::string& outJson) const     override;
    void onLoad(const std::string& json)        override;
    Type getType() const override { return Type::Mesh; }

    Model* getModel() const;

    UID getModelUID() const { return m_modelUID; }

    void           computeLocalAABB();
    bool           hasAABB()         const { return m_hasAABB; }
    const Vector3& getLocalAABBMin() const { return m_localAABBMin; }
    const Vector3& getLocalAABBMax() const { return m_localAABBMax; }
    void getWorldAABB(Vector3& outMin, Vector3& outMax) const;

private:
    UID           m_modelUID = 0;
    ResourceMesh* m_resource = nullptr;  

    std::shared_ptr<Model> m_proceduralModel;

    std::vector<ComPtr<ID3D12Resource>> m_materialBuffers;

    Vector3 m_localAABBMin = {};
    Vector3 m_localAABBMax = {};
    bool    m_hasAABB = false;
};