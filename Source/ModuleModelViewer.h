#pragma once

#include "Module.h"
#include "DebugDrawPass.h"
#include "ImGuiPass.h"
#include "Model.h"
#include <imgui.h>
#include "ImGuizmo.h"

class Model;
class Mesh;
class Material;

class ModuleModelViewer : public Module
{
    struct PerInstance
    {
        Matrix modelMat;
        Matrix normalMat;
        Material::PhongMaterial material;
    };

    struct PerFrame
    {
        Vector3 L = Vector3::UnitX;
        float pad0;
        Vector3 Lc = Vector3::One;
        float pad1;
        Vector3 Ac = Vector3::Zero;
        float pad2;
        Vector3 viewPos = Vector3::Zero;
        float pad3;
    };

    struct Light
    {
        Vector3 L = Vector3::One * (-0.5f);
        Vector3 Lc = Vector3::One;
        Vector3 Ac = Vector3::One * (0.1f);
    };

    Light light;
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pso;
    std::unique_ptr<DebugDrawPass> debugDrawPass;
    std::vector<ComPtr<ID3D12Resource>> materialBuffers;
    std::unique_ptr<Model> model;

    bool pendingPIXCapture = false;

public:
    ModuleModelViewer();
    ~ModuleModelViewer();

    virtual bool init() override;
    virtual bool cleanUp() override;
    virtual void preRender() override;
    virtual void render() override;

    void render3DContent(ID3D12GraphicsCommandList* commandList);
    void imGuiCommands();

    bool IsGridVisible() const { return showGrid; }
    bool IsAxisVisible() const { return showAxis; }
    bool IsGuizmoVisible() const { return showGuizmo; }

    bool HasModel() const { return model != nullptr; }
    const std::unique_ptr<Model>& GetModel() const { return model; }

    const Light& GetLight() const { return light; }
    void SetLight(const Light& l) { light = l; }

    Matrix GetModelMatrix() const { return model ? model->getModelMatrix() : Matrix::Identity; }
    void SetModelMatrix(const Matrix& m) { if (model) model->setModelMatrix(m); }

    void SetShowGrid(bool show) { showGrid = show; }
    void SetShowAxis(bool show) { showAxis = show; }
    void SetShowGuizmo(bool show) { showGuizmo = show; }

    bool capturePIXFrame();
    void validateGraphicsResources();

    ImGuizmo::OPERATION GetGizmoOperation() const { return gizmoOperation; }
    void SetGizmoOperation(ImGuizmo::OPERATION operation) { gizmoOperation = operation; }

private:
    bool createRootSignature();
    bool createPSO();
    bool loadModel();

    bool showAxis = false;
    bool showGrid = true;
    bool showGuizmo = true;

    ImGuizmo::OPERATION gizmoOperation = ImGuizmo::TRANSLATE;

    Matrix getNormalMatrix() const;
};