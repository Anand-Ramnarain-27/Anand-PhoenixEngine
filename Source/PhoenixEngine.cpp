// Editor entry point: the Win32 window, its message loop and the routing of window messages into the engine.
#include "Globals.h"

#include "framework.h"
#include "PhoenixEngine.h"

#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleEditor.h"
#include "SceneManager.h"
#include "ModuleAssets.h"

#include <shellapi.h>
#include <filesystem>

#include "dxgidebug.h"

#include "Keyboard.h"
#include "Mouse.h"

#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define MAX_LOADSTRING 100

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

Application* app = nullptr;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow){
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_ENGINEDX, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // OleInitialize is a superset of CoInitialize; it is required by
    // RegisterDragDrop (called later in ModuleEditor::init).
    if (FAILED(OleInitialize(nullptr))) return FALSE;

    if (!InitInstance(hInstance, nCmdShow)) return FALSE;

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ENGINEDX));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)){
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)){
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    delete app;

    OleUninitialize();

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance){
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ENGINEDX));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow){
    hInst = hInstance;

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        return FALSE;

    DragAcceptFiles(hWnd, TRUE);

    app = new Application(__argc, __wargv, hWnd);

    if (!app->init()){
        delete app;
        return FALSE;
    }

    // Fill the monitor the window opened on.
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &monitor);
    RECT rect = monitor.rcMonitor;
    SetWindowPos(hWnd, nullptr, static_cast<int>(rect.left), static_cast<int>(rect.top), static_cast<int>(rect.left + rect.right), static_cast<int>(rect.top + rect.bottom), SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    SetWindowPos(hWnd, nullptr, static_cast<int>(rect.left), static_cast<int>(rect.top), static_cast<int>(rect.left + rect.right), static_cast<int>(rect.top + rect.bottom), 0);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

static bool isModelExtension(const std::string& ext){
    return ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj" || ext == ".stl" || ext == ".blend";
}

static bool isTextureExtension(const std::string& ext){
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga" || ext == ".bmp" || ext == ".hdr";
}

static bool isAudioExtension(const std::string& ext){
    return ext == ".wav" || ext == ".mp3" || ext == ".ogg";
}

static bool isKnownAssetExtension(const std::string& ext){
    return isModelExtension(ext) || isTextureExtension(ext) || isAudioExtension(ext) || ext == ".json";
}

// A file dropped from Explorer: copied into Assets/<Models|Textures|Audio>/ (unless it is already under Assets/)
// and imported; anything else known (scenes, data) only triggers an asset refresh.
static std::string handleDroppedFile(const char* srcPath){
    if (!app || !app->getAssets() || !app->getFileSystem()) return "";

    namespace fs = std::filesystem;
    ModuleFileSystem* fsys = app->getFileSystem();

    std::string ext = fs::path(srcPath).extension().string();
    for (char& c : ext) c = (char)tolower((unsigned char)c);

    if (!isKnownAssetExtension(ext)){
        PHX_LOG(Core, Warning, "Drop: Unsupported file type '%s' (%s)", ext.c_str(), srcPath);
        return "";
    }

    std::string assetsRoot = fsys->GetAssetsPath();
    std::string subDir;
    if (isModelExtension(ext)) subDir = "Models/";
    else if (isTextureExtension(ext)) subDir = "Textures/";
    else if (isAudioExtension(ext)) subDir = "Audio/";

    std::string destDir = assetsRoot + subDir;
    fsys->CreateDir(destDir.c_str());

    std::string filename = fs::path(srcPath).filename().string();
    std::string destPath = destDir + filename;

    bool alreadyInAssets = (std::string(srcPath).rfind(assetsRoot, 0) == 0);
    if (!alreadyInAssets){
        if (!fsys->Copy(srcPath, destPath.c_str())){
            PHX_LOG(Core, Error, "Drop: Failed to copy '%s' -> '%s'", srcPath, destPath.c_str());
            return "";
        }
        PHX_LOG(Core, Info, "Drop: Copied '%s' -> '%s'", srcPath, destPath.c_str());
    }
    else {
        destPath = srcPath;
    }

    if (isModelExtension(ext) || isTextureExtension(ext)){
        UID uid = app->getAssets()->importAsset(destPath.c_str());
        if (uid != 0) PHX_LOG(Core, Info, "Drop: Imported '%s' (uid=%llu)", destPath.c_str(), uid);
        else PHX_LOG(Core, Warning, "Drop: Import returned 0 for '%s'", destPath.c_str());
    }
    else {
        app->getAssets()->refreshAssets();
    }

    return destPath;
}

// WM_SYSKEYDOWN lParam: bit 29 is set while Alt is held, bit 30 when the key was already down (auto-repeat).
constexpr LPARAM kAltDownBit = 0x20000000;
constexpr LPARAM kRepeatBit = 0x40000000;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

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
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId){
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    }
    case WM_PAINT:
        app->update();
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED){
            app->setPaused(true);
        }
        else {
            app->setPaused(false);
            app->getD3D12()->resize();
        }
        break;

    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);

        for (UINT i = 0; i < fileCount; ++i){
            WCHAR wideFilePath[MAX_PATH];
            DragQueryFileW(hDrop, i, wideFilePath, MAX_PATH);

            char narrowPath[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, wideFilePath, -1, narrowPath, MAX_PATH, nullptr, nullptr);

            handleDroppedFile(narrowPath);
        }

        DragFinish(hDrop);

        if (app && app->getAssets())
            app->getAssets()->refreshAssets();

        return 0;
    }

    case WM_SYSKEYDOWN:
        // Alt+Enter, first press only.
        if (wParam == VK_RETURN && (lParam & (kAltDownBit | kRepeatBit)) == kAltDownBit){
            app->getD3D12()->toggleFullscreen();
        }
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

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam){
    UNREFERENCED_PARAMETER(lParam);
    switch (message){
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL){
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
