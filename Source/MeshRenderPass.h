#pragma once

#include "MeshPipeline.h"
#include "MeshEntry.h"
#include <vector>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class EnvironmentSystem;
class ModuleRingBuffer;
class ModuleSamplerHeap;

class MeshRenderPass
{
public:
    MeshRenderPass() = default;
    ~MeshRenderPass() = default;

    bool init(ID3D12Device* device);

    void render(ID3D12GraphicsCommandList* cmd, const std::vector<MeshEntry*>& meshes, D3D12_GPU_VIRTUAL_ADDRESS lightCBAddr, const float viewProj[16], const EnvironmentSystem* env, ModuleSamplerHeap* samplerHeap);

    MeshPipeline& getPipeline() { return m_pipeline; }

private:
    MeshPipeline m_pipeline;
};