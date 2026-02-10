#pragma once

#include "Component.h"
#include "ModuleCamera.h"

class ComponentMeshRenderer : public Component
{
public:
    void render(
        ID3D12GraphicsCommandList* cmd,
        const ModuleCamera& camera,
        const Matrix& model) override
    {
        // Set constant buffer (model matrix)
        // Bind pipeline
        // Draw mesh
    }
};

