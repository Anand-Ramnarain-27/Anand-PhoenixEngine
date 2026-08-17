#include "Globals.h"
#include <windows.h>
#include <ole2.h>

#include "Application.h"
#include "ModuleD3D12.h"

#include "Keyboard.h"
#include "Mouse.h"

Application* app = nullptr;

static const wchar_t kWindowClassName[] = L"PhoenixPlayerWindowClass";
static const wchar_t kWindowTitle[] = L"Phoenix Player";

LRESULT CALLBACK PlayerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){
    switch (message){
    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
        Keyboard::ProcessMessage(message, wParam, lParam);
        Mouse::ProcessMessage(message, wParam, lParam);
        break;
    case WM_INPUT:
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_MOUSEHOVER:
        Mouse::ProcessMessage(message, wParam, lParam);
        break;
    case WM_PAINT:
        if (app) app->update();
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        if (app){
            if (wParam == SIZE_MINIMIZED) app->setPaused(true);
            else { app->setPaused(false); app->getD3D12()->resize(); }
        }
        break;
    case WM_SYSKEYDOWN:
        if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000 && app)
            app->getD3D12()->toggleFullscreen();
        Keyboard::ProcessMessage(message, wParam, lParam);
        break;
    case WM_KEYDOWN:
        Keyboard::ProcessMessage(message, wParam, lParam);
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        Keyboard::ProcessMessage(message, wParam, lParam);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

static ATOM RegisterPlayerWindowClass(HINSTANCE hInstance){
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = PlayerWndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = kWindowClassName;
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    return RegisterClassExW(&wcex);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow){
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // OleInitialize is a superset of CoInitialize; DirectXTex's WIC-based texture
    // import path (shared with the Editor, exercised by ModuleAssets at startup)
    // requires COM to be initialized even though the Player itself never uses
    // drag-drop.
    if (FAILED(OleInitialize(nullptr)))
        return FALSE;

    RegisterPlayerWindowClass(hInstance);

    HWND hWnd = CreateWindowW(kWindowClassName, kWindowTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd){
        OleUninitialize();
        return FALSE;
    }

    app = new Application(__argc, __wargv, hWnd);
    if (!app->init()){
        delete app;
        OleUninitialize();
        return FALSE;
    }

    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &monitor);
    RECT rect = monitor.rcMonitor;
    SetWindowPos(hWnd, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    ShowWindow(hWnd, SW_SHOWMAXIMIZED);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    delete app;
    OleUninitialize();
    return (int)msg.wParam;
}
