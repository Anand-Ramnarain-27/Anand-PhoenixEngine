#pragma once

#include "Component.h"
#include "Material.h"
#include "Model.h"
#include "ModuleD3D12.h"
#include <memory>
#include <vector>
#include <string>

class ComponentMesh : public Component
{
public:
    explicit ComponentMesh(GameObject* owner);
    ~ComponentMesh() override = default;

    bool loadModel(const char* filePath);
    void setModel(std::unique_ptr<Model> model);
    void rebuildMaterialBuffers();

    void render(ID3D12GraphicsCommandList* cmd) override;
    void onSave(std::string& outJson) const     override;
    void onLoad(const std::string& json)        override;
    Type getType() const override { return Type::Mesh; }

    Model* getModel() const { return m_model.get(); }

private:
    std::shared_ptr<Model>              m_model;
    std::vector<ComPtr<ID3D12Resource>> m_materialBuffers;
    std::string                         m_modelFilePath;
};