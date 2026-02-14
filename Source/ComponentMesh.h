#pragma once

#include "Component.h"
#include <memory>
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

private:
    std::unique_ptr<Model> m_model;
};