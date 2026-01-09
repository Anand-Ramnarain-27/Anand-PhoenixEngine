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
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pso;
    std::unique_ptr<DebugDrawPass> debugDrawPass;
    std::vector<ComPtr<ID3D12Resource>> materialBuffers;
    std::unique_ptr<Model> model;

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
    void SetShowGrid(bool show) { showGrid = show; }
    void SetShowAxis(bool show) { showAxis = show; }
    void SetShowGuizmo(bool show) { showGuizmo = show; }

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
};