#include "Globals.h"
#include "DeferredLightingPass.h"
#include "EnvironmentSystem.h"
#include "EnvironmentMap.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleGPUResources.h"
#include "ReadData.h"
#include <d3dx12.h>

bool DeferredLightingPass::createPSO(ID3D12Device* device) {
    auto vs = DX::ReadData(L"FullScreenVS.cso");      
    auto ps = DX::ReadData(L"DeferredLightingPS.cso"); 

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
     
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
     
    desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
     
    desc.InputLayout = { nullptr, 0 };

    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = UINT_MAX;

    return SUCCEEDED(device->CreateGraphicsPipelineState(
        &desc, IID_PPV_ARGS(&m_pso)));
}

void DeferredLightingPass::render(
    ID3D12GraphicsCommandList* cmd,
    GBuffer& gbuffer,
    ID3D12Resource* depthResource,
    const FrameLightData& lights,
    const Vector3& cameraPos,
    const Matrix& view,
    const Matrix& proj,
    const EnvironmentSystem* env) {
     
    auto copy = [](void* dst, const void* src, size_t n, size_t stride,
        size_t maxN) {
            memcpy(dst, src, std::min(n, maxN) * stride); };
    using DL = MeshPipeline::GPUDirectionalLight;
    using PL = MeshPipeline::GPUPointLight;
    using SL = MeshPipeline::GPUSpotLight;
    copy(m_dirMapped, lights.dirLights.data(),
        lights.dirLights.size(), sizeof(DL), MeshPipeline::MAX_DIR_LIGHTS);
    copy(m_pointMapped, lights.pointLights.data(),
        lights.pointLights.size(), sizeof(PL), MeshPipeline::MAX_POINT_LIGHTS);
    copy(m_spotMapped, lights.spotLights.data(),
        lights.spotLights.size(), sizeof(SL), MeshPipeline::MAX_SPOT_LIGHTS);
     
    LightCB cb{};
    Matrix vp = view * proj;
    Matrix invVP = vp.Invert();
    cb.invViewProj = invVP.Transpose();
    cb.cameraPosition = cameraPos;
    cb.dirLightCount = (uint32_t)std::min(
        lights.dirLights.size(), (size_t)MeshPipeline::MAX_DIR_LIGHTS);
    cb.pointLightCount = (uint32_t)std::min(
        lights.pointLights.size(), (size_t)MeshPipeline::MAX_POINT_LIGHTS);
    cb.spotLightCount = (uint32_t)std::min(
        lights.spotLights.size(), (size_t)MeshPipeline::MAX_SPOT_LIGHTS);
    cb.envRoughnessLevels = (env && env->hasIBL())
        ? EnvironmentMap::NUM_ROUGHNESS_LEVELS : 0;
    memcpy(m_lightMapped, &cb, sizeof(cb));
     
    auto* sd = app->getShaderDescriptors();
    auto* samplers = app->getSamplerHeap();
    ID3D12DescriptorHeap* heaps[] = { sd->getHeap(), samplers->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);
    cmd->SetPipelineState(m_pso.Get());
    cmd->SetGraphicsRootSignature(m_rootSig.Get());
     
    cmd->SetGraphicsRootConstantBufferView(0,
        m_lightCB->GetGPUVirtualAddress());
     
    cmd->SetGraphicsRootDescriptorTable(1, gbuffer.getAlbedoSRV()); 
     
    if (env && env->hasIBL()) {
        auto* map = env->getEnvironmentMap();
        cmd->SetGraphicsRootDescriptorTable(2, map->getIrradianceGPU());
    }
    else {
        cmd->SetGraphicsRootDescriptorTable(2,
            m_fallbackIBLTable.getGPUHandle(0));
    }

    // t7-t9 = light structured buffers
    cmd->SetGraphicsRootDescriptorTable(3,
        m_lightSRVTable.getGPUHandle(0));

    // samplers (point clamp + linear wrap)
    cmd->SetGraphicsRootDescriptorTable(4,
        samplers->getGPUHandle(ModuleSamplerHeap::POINT_CLAMP));
     
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->DrawInstanced(3, 1, 0, 0);
}
