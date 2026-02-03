#pragma once
#include "Module.h"
#include <dxgi1_6.h>

class ModuleD3D12 : public Module
{
public:
    ModuleD3D12(HWND hWnd);
    ~ModuleD3D12() override;

    bool init() override;
    bool cleanUp() override;
    void preRender() override;
    void postRender() override;

    void resize();
    void toggleFullscreen();
    void flush();

    HWND getHWnd() const { return m_hWnd; }
    IDXGISwapChain4* getSwapChain() const { return m_swapChain.Get(); }
    ID3D12Device5* getDevice() const { return m_device.Get(); }
    ID3D12GraphicsCommandList* getCommandList() const { return m_commandList.Get(); }
    ID3D12CommandAllocator* getCommandAllocator() const { return m_commandAllocators[m_currentBackBufferIdx].Get(); }
    ID3D12Resource* getBackBuffer() const { return m_backBuffers[m_currentBackBufferIdx].Get(); }
    ID3D12CommandQueue* getDrawCommandQueue() const { return m_drawCommandQueue.Get(); }

    unsigned getCurrentBackBufferIdx() const { return m_currentBackBufferIdx; }
    unsigned getCurrentFrame() const { return m_frameIndex; }
    unsigned getLastCompletedFrame() const { return m_lastCompletedFrame; }
    unsigned getWindowWidth() const { return m_windowWidth; }
    unsigned getWindowHeight() const { return m_windowHeight; }
    bool isFullscreen() const { return m_fullscreen; }

    D3D12_CPU_DESCRIPTOR_HANDLE getRenderTargetDescriptor() const;
    D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilDescriptor() const;

    UINT64 signalDrawQueue();
    ID3D12GraphicsCommandList* beginFrameRender();
    void setBackBufferRenderTarget(const Vector4& clearColor = Vector4(0.0f, 0.0f, 0.0f, 1.0f));
    void endFrameRender();

    bool useVSync = true;

    D3D12_CPU_DESCRIPTOR_HANDLE createRTV(ID3D12Resource* resource);
    D3D12_CPU_DESCRIPTOR_HANDLE createDSV(ID3D12Resource* resource);
private:
    void enableDebugLayer();
    bool createFactory();
    bool createDevice(bool useWarp);
    bool setupInfoQueue();
    bool createDrawCommandQueue();
    bool createSwapChain();
    bool createRenderTargets();
    bool createDepthStencil();
    bool createCommandList();
    bool createDrawFence();

    void getWindowSize(unsigned& width, unsigned& height) const;
    void waitForFrameFence(UINT64 fenceValue);

private:
    HWND m_hWnd = nullptr;
    unsigned m_windowWidth = 0;
    unsigned m_windowHeight = 0;
    bool m_fullscreen = false;
    RECT m_lastWindowRect = {};

    ComPtr<IDXGIFactory6> m_factory;
    ComPtr<IDXGIAdapter4> m_adapter;
    ComPtr<ID3D12Device5> m_device;
    ComPtr<IDXGISwapChain4> m_swapChain;

    ComPtr<ID3D12DescriptorHeap> m_rtDescriptorHeap;
    ComPtr<ID3D12Resource> m_backBuffers[FRAMES_IN_FLIGHT];
    ComPtr<ID3D12DescriptorHeap> m_dsDescriptorHeap;
    ComPtr<ID3D12Resource> m_depthStencilBuffer;

    ComPtr<ID3D12CommandAllocator> m_commandAllocators[FRAMES_IN_FLIGHT];
    ComPtr<ID3D12GraphicsCommandList4> m_commandList;
    ComPtr<ID3D12CommandQueue> m_drawCommandQueue;

    ComPtr<ID3D12Fence1> m_drawFence;
    HANDLE m_drawEvent = nullptr;
    UINT64 m_drawFenceCounter = 1; // Start at 1 to avoid 0
    UINT64 m_drawFenceValues[FRAMES_IN_FLIGHT] = { 1, 1, 1 };

    unsigned m_frameValues[FRAMES_IN_FLIGHT] = { 0, 0, 0 };
    unsigned m_frameIndex = 0;
    unsigned m_lastCompletedFrame = 0;
    unsigned m_currentBackBufferIdx = 0;

    bool m_allowTearing = false;
    bool m_supportsRT = false;
};