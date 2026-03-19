#pragma once
#include "ModuleD3D12.h"
#include "FaceProjection.h"
#include "Application.h"
#include "ModuleRTDescriptors.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"

class CubeFaceRenderer {
public:
    struct RenderParams {
        ID3D12GraphicsCommandList* cmd;
        ID3D12Resource* target;
        uint32_t faceIndex;
        uint32_t mipLevel;
        uint32_t totalMips;
        uint32_t baseFaceSize;
        float roughness;
        ID3D12RootSignature* rootSig;
        ID3D12PipelineState* pso;
        D3D12_GPU_DESCRIPTOR_HANDLE sourceSRV;
        DXGI_FORMAT rtvFormat;
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress;
        D3D12_GPU_DESCRIPTOR_HANDLE samplerHandle;
    };

    static void RenderFace(const RenderParams& params) {
        auto* rtDescs = app->getRTDescriptors();
        auto* samplers = app->getSamplerHeap();
        uint32_t mipSize = std::max(1u, params.baseFaceSize >> params.mipLevel);

        RenderTargetDesc rtv = rtDescs->create(params.target, params.faceIndex, params.mipLevel, params.rtvFormat);
        if (!rtv) return;

        UINT subRes = D3D12CalcSubresource(params.mipLevel, params.faceIndex, 0, params.totalMips, 6);
        auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(params.target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, subRes);
        params.cmd->ResourceBarrier(1, &toRT);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtv.getCPUHandle();
        params.cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        float clear[4] = { 0, 0, 0, 1 };
        params.cmd->ClearRenderTargetView(rtvHandle, clear, 0, nullptr);

        D3D12_VIEWPORT vp = { 0, 0, float(mipSize), float(mipSize), 0, 1 };
        D3D12_RECT sc = { 0, 0, LONG(mipSize), LONG(mipSize) };
        params.cmd->RSSetViewports(1, &vp);
        params.cmd->RSSetScissorRects(1, &sc);

        ID3D12DescriptorHeap* heaps[] = {app->getShaderDescriptors()->getHeap(), samplers->getHeap()};
        params.cmd->SetDescriptorHeaps(2, heaps);
        params.cmd->SetGraphicsRootSignature(params.rootSig);
        params.cmd->SetPipelineState(params.pso);
        params.cmd->SetGraphicsRootConstantBufferView(0, params.cbAddress);
        params.cmd->SetGraphicsRootDescriptorTable(1, params.sourceSRV);
        params.cmd->SetGraphicsRootDescriptorTable(2, params.samplerHandle);

        auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(params.target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, subRes);
        params.cmd->ResourceBarrier(1, &toSRV);
    }
};