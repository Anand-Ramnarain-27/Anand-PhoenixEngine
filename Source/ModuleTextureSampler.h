#pragma once

#include "Module.h"
#include "DebugDrawPass.h"
#include "ImGuiPass.h"
#include "GraphicsSamplers.h" 
#include <array> 

class ModuleTextureSampler : public Module
{
    ComPtr<ID3D12Resource>          textureDog;
    ComPtr<ID3D12DescriptorHeap>    srvHeap;
    D3D12_GPU_DESCRIPTOR_HANDLE     srvGPUHandle;

    ComPtr<ID3D12Resource>          vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW        vertexBufferView;
    ComPtr<ID3D12RootSignature>     rootSignature;
    ComPtr<ID3D12PipelineState>     pso;
    std::unique_ptr<DebugDrawPass>  debugDrawPass;
    std::unique_ptr<ImGuiPass>      imguiPass;

    bool                            showAxis = true;
    bool                            showGrid = true;
    int                             currentSampler = 0;

public:
    virtual bool init() override;
    virtual bool cleanUp() override;
    virtual void preRender() override;
    virtual void render() override;

private:
    bool createVertexBuffer(void* bufferData, unsigned bufferSize, unsigned stride);
    bool createRootSignature();
    bool createPSO();
    bool loadTexture();
};