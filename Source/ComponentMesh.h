#pragma once

#include "Component.h"
#include "Material.h" 
#include "Model.h"
#include <memory>
#include <vector>
#include <string>
#include "ModuleD3D12.h"

class Model;

class ComponentMesh : public Component
{
public:
    ComponentMesh(GameObject* owner);
    ~ComponentMesh() override;

    bool loadModel(const char* filePath);

    void render(ID3D12GraphicsCommandList* cmd) override;

    Model* getModel() const { return m_model.get(); }

    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::Mesh; }

    void rebuildMaterialBuffers();
    void setModel(std::unique_ptr<Model> model);
private:
    std::shared_ptr<Model> m_model;
    std::vector<ComPtr<ID3D12Resource>> m_materialBuffers;
    std::string m_modelFilePath;
};