#include "Globals.h"
#include "ModuleD3D12.h"
#include "Application.h"
#include "d3dx12.h"

ModuleD3D12::ModuleD3D12(HWND wnd) : m_hWnd(wnd) {}
ModuleD3D12::~ModuleD3D12() { cleanUp(); }

bool ModuleD3D12::init()
{
#if defined(_DEBUG)
    enableDebugLayer();
#endif
    getWindowSize(m_windowWidth, m_windowHeight);

    if (!createFactory())          return false;
    if (!createDevice(false))      return false;
#if defined(_DEBUG)
    if (!setupInfoQueue())         return false;
#endif
    if (!createDrawCommandQueue()) return false;
    if (!createSwapChain())        return false;
    if (!createRenderTargets())    return false;
    if (!createDepthStencil())     return false;
    if (!createCommandList())      return false;
    if (!createDrawFence())        return false;

    m_currentBackBufferIdx = m_swapChain->GetCurrentBackBufferIndex();
    signalDrawQueue();
    return true;
}

bool ModuleD3D12::cleanUp()
{
    flush();
    if (m_drawEvent) { CloseHandle(m_drawEvent); m_drawEvent = nullptr; }
    return true;
}

void ModuleD3D12::preRender()
{
    m_currentBackBufferIdx = m_swapChain->GetCurrentBackBufferIndex();
    waitForFrameFence(m_drawFenceValues[m_currentBackBufferIdx]);

    m_frameValues[m_currentBackBufferIdx] = ++m_frameIndex;

    const UINT64 completed = m_drawFence->GetCompletedValue();
    for (unsigned i = 0; i < FRAMES_IN_FLIGHT; ++i)
        if (m_frameValues[i] > m_lastCompletedFrame && m_drawFenceValues[i] <= completed)
            m_lastCompletedFrame = m_frameValues[i];

    m_commandAllocators[m_currentBackBufferIdx]->Reset();
}

void ModuleD3D12::postRender()
{
    m_swapChain->Present(useVSync ? 1 : 0, (!useVSync && m_allowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0);
    signalDrawQueue();
}

UINT64 ModuleD3D12::signalDrawQueue()
{
    m_drawFenceValues[m_currentBackBufferIdx] = ++m_drawFenceCounter;
    m_drawCommandQueue->Signal(m_drawFence.Get(), m_drawFenceValues[m_currentBackBufferIdx]);
    return m_drawFenceCounter;
}

void ModuleD3D12::resize()
{
    unsigned w, h;
    getWindowSize(w, h);
    if (w == m_windowWidth && h == m_windowHeight) return;

    m_windowWidth = w;
    m_windowHeight = h;
    if (m_windowWidth == 0 || m_windowHeight == 0) return;

    flush();

    for (auto& bb : m_backBuffers) bb.Reset();
    m_depthStencilBuffer.Reset();
    m_dsDescriptorHeap.Reset();
    m_rtDescriptorHeap.Reset();

    DXGI_SWAP_CHAIN_DESC desc = {};
    if (FAILED(m_swapChain->GetDesc(&desc))) { LOG("Failed to get swap chain desc"); return; }

    m_commandList->Reset(m_commandAllocators[m_currentBackBufferIdx].Get(), nullptr);
    m_commandList->Close();

    if (FAILED(m_swapChain->ResizeBuffers(FRAMES_IN_FLIGHT, w, h, desc.BufferDesc.Format, desc.Flags)))
    {
        LOG("Failed to resize swap chain"); return;
    }

    m_currentBackBufferIdx = m_swapChain->GetCurrentBackBufferIndex();

    if (!createRenderTargets()) LOG("Failed to recreate render targets");
    if (!createDepthStencil())  LOG("Failed to recreate depth stencil");
}

void ModuleD3D12::toggleFullscreen()
{
    m_fullscreen = !m_fullscreen;

    if (m_fullscreen)
    {
        GetWindowRect(m_hWnd, &m_lastWindowRect);
        UINT style = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        SetWindowLongW(m_hWnd, GWL_STYLE, style);

        MONITORINFOEX mi = {}; mi.cbSize = sizeof(mi);
        GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowPos(m_hWnd, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);
        ShowWindow(m_hWnd, SW_MAXIMIZE);
    }
    else
    {
        SetWindowLong(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowPos(m_hWnd, HWND_NOTOPMOST,
            m_lastWindowRect.left, m_lastWindowRect.top,
            m_lastWindowRect.right - m_lastWindowRect.left,
            m_lastWindowRect.bottom - m_lastWindowRect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);
        ShowWindow(m_hWnd, SW_NORMAL);
    }
}

void ModuleD3D12::flush()
{
    if (!m_drawCommandQueue || !m_drawFence) return;
    m_drawCommandQueue->Signal(m_drawFence.Get(), ++m_drawFenceCounter);
    waitForFrameFence(m_drawFenceCounter);
}

void ModuleD3D12::waitForFrameFence(UINT64 fenceValue)
{
    if (m_drawFence->GetCompletedValue() < fenceValue && m_drawEvent)
    {
        m_drawFence->SetEventOnCompletion(fenceValue, m_drawEvent);
        WaitForSingleObject(m_drawEvent, INFINITE);
    }
}

void ModuleD3D12::enableDebugLayer()
{
    ComPtr<ID3D12Debug> dbg;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) dbg->EnableDebugLayer();
}

bool ModuleD3D12::createFactory()
{
    UINT flags = 0;
#if defined(_DEBUG)
    flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    return SUCCEEDED(CreateDXGIFactory2(flags, IID_PPV_ARGS(&m_factory)));
}

bool ModuleD3D12::createDevice(bool useWarp)
{
    if (useWarp)
    {
        ComPtr<IDXGIAdapter1> warp;
        return SUCCEEDED(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)))
            && SUCCEEDED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
    }

    if (FAILED(m_factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_adapter)))) return false;
    if (FAILED(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device))))                      return false;

    BOOL tearing = FALSE;
    m_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing));
    m_allowTearing = tearing == TRUE;

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5 = {};
    if (SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opts5, sizeof(opts5))))
        m_supportsRT = opts5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;

    return true;
}

bool ModuleD3D12::setupInfoQueue()
{
    ComPtr<ID3D12InfoQueue> iq;
    if (FAILED(m_device.As(&iq))) return false;

    iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

    D3D12_MESSAGE_ID hide[] = {
        D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
        D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
    };
    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.NumIDs = _countof(hide);
    filter.DenyList.pIDList = hide;
    iq->AddStorageFilterEntries(&filter);
    return true;
}

bool ModuleD3D12::createDrawCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    return SUCCEEDED(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_drawCommandQueue)));
}

bool ModuleD3D12::createSwapChain()
{
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = m_windowWidth;
    desc.Height = m_windowHeight;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc = { 1, 0 };
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = FRAMES_IN_FLIGHT;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags = m_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> sc1;
    if (FAILED(m_factory->CreateSwapChainForHwnd(m_drawCommandQueue.Get(), m_hWnd, &desc, nullptr, nullptr, &sc1))) return false;
    if (FAILED(sc1.As(&m_swapChain))) return false;
    m_factory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);
    return true;
}

bool ModuleD3D12::createDepthStencil()
{
    D3D12_CLEAR_VALUE cv = {};
    cv.Format = DXGI_FORMAT_D32_FLOAT;
    cv.DepthStencil.Depth = 1.0f;

    CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC   rd = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT,
        m_windowWidth, m_windowHeight, 1, 0, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);

    if (FAILED(m_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS(&m_depthStencilBuffer)))) return false;

    D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
    dhd.NumDescriptors = 1;
    dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    if (FAILED(m_device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&m_dsDescriptorHeap)))) return false;

    m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), nullptr,
        m_dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

bool ModuleD3D12::createRenderTargets()
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = FRAMES_IN_FLIGHT;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (FAILED(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtDescriptorHeap)))) return false;

    const UINT stride = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    for (int i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])))) return false;
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, rtv);
        rtv.ptr += stride;
    }
    return true;
}

bool ModuleD3D12::createCommandList()
{
    for (UINT i = 0; i < FRAMES_IN_FLIGHT; ++i)
        if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_commandAllocators[i])))) return false;

    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_commandList)))) return false;

    m_commandList->Close();
    return true;
}

bool ModuleD3D12::createDrawFence()
{
    if (FAILED(m_device->CreateFence(m_drawFenceCounter, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_drawFence)))) return false;
    m_drawEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    return m_drawEvent != nullptr;
}

void ModuleD3D12::getWindowSize(UINT& width, UINT& height) const
{
    RECT r = {};
    GetClientRect(m_hWnd, &r);
    width = (UINT)(r.right - r.left);
    height = (UINT)(r.bottom - r.top);
}

D3D12_CPU_DESCRIPTOR_HANDLE ModuleD3D12::getRenderTargetDescriptor() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_rtDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        m_currentBackBufferIdx,
        m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
}

D3D12_CPU_DESCRIPTOR_HANDLE ModuleD3D12::getDepthStencilDescriptor() const
{
    return m_dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
}

ID3D12GraphicsCommandList* ModuleD3D12::beginFrameRender()
{
    m_commandList->Reset(getCommandAllocator(), nullptr);
    return m_commandList.Get();
}

void ModuleD3D12::setBackBufferRenderTarget(const Vector4& clearColor)
{
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(getBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrier);

    auto rtv = getRenderTargetDescriptor();
    auto dsv = getDepthStencilDescriptor();

    m_commandList->OMSetRenderTargets(1, &rtv, false, &dsv);
    m_commandList->ClearRenderTargetView(rtv, &clearColor.x, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    D3D12_VIEWPORT vp{ 0.f, 0.f, float(m_windowWidth), float(m_windowHeight), 0.f, 1.f };
    D3D12_RECT     sc{ 0, 0, LONG(m_windowWidth), LONG(m_windowHeight) };
    m_commandList->RSSetViewports(1, &vp);
    m_commandList->RSSetScissorRects(1, &sc);
}

D3D12_CPU_DESCRIPTOR_HANDLE ModuleD3D12::createRTV(ID3D12Resource* resource)
{
    auto handle = m_rtDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_device->CreateRenderTargetView(resource, nullptr, handle);
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE ModuleD3D12::createDSV(ID3D12Resource* resource)
{
    auto handle = m_dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_device->CreateDepthStencilView(resource, nullptr, handle);
    return handle;
}

void ModuleD3D12::endFrameRender()
{
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(getBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrier);

    if (SUCCEEDED(m_commandList->Close()))
    {
        ID3D12CommandList* lists[] = { m_commandList.Get() };
        m_drawCommandQueue->ExecuteCommandLists(1, lists);
    }
}