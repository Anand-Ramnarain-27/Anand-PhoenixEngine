#pragma once

#include <wrl.h>
#include <memory>
#include "Mesh.h"
#include "EnvironmentMap.h"

using Microsoft::WRL::ComPtr;

class SkyboxRenderer
{
public:
    bool init(ID3D12Device* device);

    void render(
        ID3D12GraphicsCommandList* cmd,
        const EnvironmentMap& env,
        const Matrix& view,
        const Matrix& projection);

private:
    bool createRootSignature(ID3D12Device* device);
    bool createPSO(ID3D12Device* device);
    void createCube();

private:
    ComPtr<ID3D12RootSignature> rootSig;
    ComPtr<ID3D12PipelineState> pso;
    std::unique_ptr<Mesh>       cube;
};