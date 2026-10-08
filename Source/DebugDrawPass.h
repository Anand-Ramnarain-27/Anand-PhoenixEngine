#pragma once
// Debug line, point and text rendering (the debug_draw library on D3D12).

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>

class DDRenderInterfaceCoreD3D12;

/// Renders the debug_draw library's queued lines, points and text (dd:: calls) with D3D12.
class DebugDrawPass
{

public:

    DebugDrawPass(ID3D12Device4* device, ID3D12CommandQueue* uploadQueue, bool useMSAA, D3D12_CPU_DESCRIPTOR_HANDLE cpuText = { 0 }, D3D12_GPU_DESCRIPTOR_HANDLE gpuText = { 0 });
    ~DebugDrawPass();

    void record(ID3D12GraphicsCommandList* commandList, uint32_t width, uint32_t height, const Matrix& view, const Matrix& proj);

private:

    static DDRenderInterfaceCoreD3D12* implementation;
};
