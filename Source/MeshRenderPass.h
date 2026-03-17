#pragma once

#include "MeshPipeline.h"
#include "MeshEntry.h"
#include "ShaderTableDesc.h"
#include <vector>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class EnvironmentSystem;
class ModuleSamplerHeap;

class MeshRenderPass
{
public:
    MeshRenderPass() = default;
    ~MeshRenderPass() = default;

    // useMSAA must match the render target sample count this pass draws into
    bool init(ID3D12Device* device, bool useMSAA = false);

    void render(
        ID3D12GraphicsCommandList* cmd,
        const std::vector<MeshEntry*>& meshes,
        D3D12_GPU_VIRTUAL_ADDRESS       lightCBAddr,
        const float                     viewProj[16],
        const EnvironmentSystem* env,
        ModuleSamplerHeap* samplerHeap);

    MeshPipeline& getPipeline() { return m_pipeline; }

private:
    MeshPipeline m_pipeline;

    // 1x1 white texture used as a fallback for any optional texture slot that a
    // mesh doesn't have. D3D12 requires all descriptor tables in the root
    // signature to point to valid descriptors even if the shader won't read them
    // (guarded by has* flags). Without this, the debug layer fires a break the
    // moment a slot is first referenced - which causes the black-mesh symptom
    // when IBL enables and the shader starts executing more code paths.
    ComPtr<ID3D12Resource> m_fallbackTex;
    ShaderTableDesc        m_fallbackTable;

    bool createFallbackTexture(ID3D12Device* device);
};