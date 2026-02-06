#pragma once

#include "DebugDrawPass.h"
#include "ImGuiPass.h"
#include "Module.h"
#include <imgui.h>
#include "ImGuizmo.h"
#include "Material.h"
#include "RenderTexture.h"
#include "ShaderTableDesc.h"

class Model;

class ViewportDemo : public Module
{
    struct PerInstance
    {
        Matrix modelMat;
        Matrix normalMat;

        Material::Data material;
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

    Light                               light;
    ComPtr<ID3D12RootSignature>         rootSignature;
    ComPtr<ID3D12PipelineState>         pso;
    std::unique_ptr<DebugDrawPass>      debugDrawPass;
    std::unique_ptr<ImGuiPass>          imguiPass;
    bool                                showAxis = false;
    bool                                showGrid = true;
    bool                                showGuizmo = false;
    ImGuizmo::OPERATION                 gizmoOperation = ImGuizmo::TRANSLATE;
    std::unique_ptr<Model>              model;

    std::unique_ptr<RenderTexture>      renderTexture;
    ImVec2                              canvasSize;
    ImVec2                              canvasPos;

    ShaderTableDesc descTable;

public:
    ViewportDemo();
    ~ViewportDemo();

    virtual bool init() override;
    virtual bool cleanUp() override;
    virtual void preRender() override;
    virtual void render() override;

private:
    void imGuiCommands();
    bool createRootSignature();
    bool createPSO();
    bool loadModel();
    void renderToTexture(ID3D12GraphicsCommandList* commandList);

    static Matrix getNormalMatrix(const Matrix& modelMatrix) {
        return modelMatrix.Invert().Transpose();
    }

};